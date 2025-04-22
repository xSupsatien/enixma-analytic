#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <curl/curl.h>
#include <gio/gio.h>

#include "imwrite.h"
#include "detection.h"

#define IMAGE_PATH "/usr/local/packages/enixma_analytic/html/images/incident/%s.jpg"

char *incident_images[MAX_IMAGES];
int incident_images_size = 0;

static char *parse_credentials(GVariant *result)
{
    char *credentials_string = NULL;
    char *id = NULL;
    char *password = NULL;

    g_variant_get(result, "(&s)", &credentials_string);
    if (sscanf(credentials_string, "%m[^:]:%ms", &id, &password) != 2)
        syslog(LOG_ERR, "Error parsing credential string '%s'", credentials_string);
    char *credentials = g_strdup_printf("%s:%s", id, password);

    free(id);
    free(password);
    return credentials;
}

static char *retrieve_vapix_credentials(const char *username)
{
    GError *error = NULL;
    GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!connection)
        syslog(LOG_ERR, "Error connecting to D-Bus: %s", error->message);

    const char *bus_name = "com.axis.HTTPConf1";
    const char *object_path = "/com/axis/HTTPConf1/VAPIXServiceAccounts1";
    const char *interface_name = "com.axis.HTTPConf1.VAPIXServiceAccounts1";
    const char *method_name = "GetCredentials";

    GVariant *result = g_dbus_connection_call_sync(connection,
                                                   bus_name,
                                                   object_path,
                                                   interface_name,
                                                   method_name,
                                                   g_variant_new("(s)", username),
                                                   NULL,
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);
    if (!result)
        syslog(LOG_ERR, "Error invoking D-Bus method: %s", error->message);

    char *credentials = parse_credentials(result);

    g_variant_unref(result);
    g_object_unref(connection);
    return credentials;
}

static size_t write_callback_binary(char *contents, size_t size, size_t nmemb, void *userdata)
{
    GByteArray *response = (GByteArray *)userdata;
    size_t total_size = size * nmemb;
    g_byte_array_append(response, (guint8 *)contents, total_size);
    return total_size;
}

static char *perform_curl_request_binary(CURL *handle, const char *url, GByteArray *response)
{
    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback_binary);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, response);

    CURLcode res = curl_easy_perform(handle);

    return (res == CURLE_OK) ? (char *)response->data : NULL;
}

static GByteArray *vapix_get_image_binary(CURL *handle, const char *credentials, const char *endpoint)
{
    GByteArray *response = g_byte_array_new(); // Create a binary buffer
    char *url = g_strdup_printf("http://127.0.0.12/%s", endpoint);

    curl_easy_setopt(handle, CURLOPT_USERPWD, credentials);
    curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);

    // You would need to adapt perform_curl_request to write into GByteArray
    char *result = perform_curl_request_binary(handle, url, response); // Adapt for binary handling
    g_free(url);

    return result ? response : NULL;
}

static GByteArray *get_image_binary(CURL *handle, const char *credentials)
{
    const char *endpoint = g_strdup_printf("axis-cgi/jpg/image.cgi?resolution=1024x768");
    return vapix_get_image_binary(handle, credentials, endpoint);
}

void imwrite(char *filename, void *img)
{
    (void)img;

    int filepath_length = snprintf(NULL, 0, IMAGE_PATH, filename);
    char *filepath = (char *)malloc(filepath_length + 1);
    if (filepath == NULL)
    {
        return;
    }

    snprintf(filepath, filepath_length + 1, IMAGE_PATH, filename);

    CURL *handle = curl_easy_init();
    char *credentials = retrieve_vapix_credentials("enixma-user");

    GByteArray *image_binary = get_image_binary(handle, credentials);

    if (image_binary)
    {
        jpeg_to_file(filepath, image_binary->data, image_binary->len);

        // Create a full copy of the filename
        char *filename_copy = strdup(filename);
        if (filename_copy == NULL)
        {
            free(filepath);
            return;
        }

        if (incident_images_size < MAX_IMAGES)
        {
            // Add to the end of the array
            incident_images[incident_images_size] = filename_copy;
            incident_images_size++;
        }
        else
        {
            // Array is full, remove the oldest entry
            if (incident_images[0] != NULL)
            {
                free(incident_images[0]);
            }

            // Shift all elements
            for (int i = 0; i < MAX_IMAGES - 1; i++)
            {
                incident_images[i] = incident_images[i + 1];
            }

            // Add new image at the end
            incident_images[MAX_IMAGES - 1] = filename_copy;
        }

        // Log updated array for debugging
        // syslog(LOG_INFO, "Updated incident_images array after adding new image:");
        // for (int i = 0; i < incident_images_size; i++) {
        //     if (incident_images[i] != NULL) {
        //         syslog(LOG_INFO, "  incident_images[%d]: %s", i, incident_images[i]);
        //     } else {
        //         syslog(LOG_INFO, "  incident_images[%d]: NULL", i);
        //     }
        // }

        const char *incident_images_filename = "/usr/local/packages/enixma_analytic/localdata/incidentImages.json";
        save_image_name(incident_images_filename, incident_images, incident_images_size);

        // Clean up the directory to remove unused image files
        cleanup_incident_images_directory();

        free(filepath);
    }

    free(credentials);
    curl_easy_cleanup(handle);
}

bool save_image_name(const char *filename, char **image_names, int array_size)
{
    if (!filename || !image_names || array_size <= 0)
        return false;

    json_t *root = json_object();
    json_t *images_json_array = json_array();

    if (!root || !images_json_array)
    {
        json_decref(root);
        return false;
    }

    // Add all image names to the JSON array
    for (int i = 0; i < array_size; i++)
    {
        if (image_names[i])
        {
            json_array_append_new(images_json_array, json_string(image_names[i]));
        }
    }

    json_object_set_new(root, "type", json_string("Incidents"));
    json_object_set_new(root, "quantity", images_json_array);
    json_object_set_new(root, "size", json_integer(array_size));

    int result = json_dump_file(root, filename, JSON_COMPACT | JSON_ENSURE_ASCII);
    json_decref(root);

    return (result == 0);
}

bool load_image_name(const char *filename, char **image_names, int array_size)
{
    if (!filename || !image_names || array_size <= 0)
        return false;

    // First, ensure we have clean pointers
    for (int i = 0; i < array_size; i++)
    {
        if (image_names[i])
        {
            free(image_names[i]);
            image_names[i] = NULL;
        }
    }

    // Load the JSON file
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);

    if (!root)
    {
        // syslog(LOG_ERR, "Failed to load image names from %s: %s", filename, error.text);
        return false;
    }

    // Get the quantity array (which contains image names)
    json_t *quantity = json_object_get(root, "quantity");

    if (!quantity || !json_is_array(quantity))
    {
        syslog(LOG_ERR, "Invalid format in %s: 'quantity' is not an array", filename);
        json_decref(root);
        return false;
    }

    // Get the size value
    json_t *size = json_object_get(root, "size");
    if (size && json_is_integer(size))
    {
        incident_images_size = (int)json_integer_value(size);
        // Ensure size doesn't exceed array capacity
        if (incident_images_size > array_size)
        {
            incident_images_size = array_size;
        }
    }
    else
    {
        // If size field is not found, calculate it from the quantity array
        incident_images_size = (int)json_array_size(quantity);
        if (incident_images_size > array_size)
        {
            incident_images_size = array_size;
        }
    }

    // Calculate how many elements to process
    size_t json_size = json_array_size(quantity);
    size_t elements_to_read = (json_size < (size_t)array_size) ? json_size : (size_t)array_size;

    // Load image names into the provided array
    for (size_t i = 0; i < elements_to_read; i++)
    {
        json_t *value = json_array_get(quantity, i);

        if (json_is_string(value))
        {
            const char *str_value = json_string_value(value);
            if (str_value)
            {
                image_names[i] = strdup(str_value);

                if (!image_names[i])
                {
                    syslog(LOG_ERR, "Memory allocation failed for image name at index %zu", i);
                    // Cleanup previously allocated strings
                    for (size_t j = 0; j < i; j++)
                    {
                        free(image_names[j]);
                        image_names[j] = NULL;
                    }
                    incident_images_size = 0;
                    json_decref(root);
                    return false;
                }
            }
        }
    }

    // Log loaded array for debugging
    // syslog(LOG_INFO, "Loaded incident_images array with size %d:", incident_images_size);
    // for (int i = 0; i < incident_images_size; i++) {
    //     if (image_names[i] != NULL) {
    //         syslog(LOG_INFO, "  incident_images[%d]: %s", i, image_names[i]);
    //     } else {
    //         syslog(LOG_INFO, "  incident_images[%d]: NULL", i);
    //     }
    // }

    json_decref(root);
    return true;
}

void cleanup_incident_images_directory(void)
{
    const char *dir_path = "/usr/local/packages/enixma_analytic/html/images/incident";
    DIR *dir;
    struct dirent *entry;
    char filepath[PATH_MAX];

    // Open the directory
    dir = opendir(dir_path);
    if (dir == NULL)
    {
        syslog(LOG_ERR, "Failed to open directory %s: %s", dir_path, strerror(errno));
        return;
    }

    // syslog(LOG_INFO, "Starting cleanup of incident images directory");

    // Read directory entries
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip . and .. directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Skip if the file doesn't have a .jpg extension
        char *ext = strrchr(entry->d_name, '.');
        if (ext == NULL || strcmp(ext, ".jpg") != 0)
        {
            continue;
        }

        // Extract the filename without extension
        char filename[256] = {0};
        size_t name_len = ext - entry->d_name;
        if (name_len >= sizeof(filename))
        {
            name_len = sizeof(filename) - 1;
        }
        strncpy(filename, entry->d_name, name_len);
        filename[name_len] = '\0';

        // Check if the filename is in our incident_images array
        bool found = false;
        for (int i = 0; i < incident_images_size; i++)
        {
            if (incident_images[i] != NULL && strcmp(incident_images[i], filename) == 0)
            {
                found = true;
                break;
            }
        }

        // If not found, delete the file
        if (!found)
        {
            snprintf(filepath, PATH_MAX, "%s/%s", dir_path, entry->d_name);

            // if (unlink(filepath) == 0) {
            //     syslog(LOG_INFO, "Deleted unused incident image: %s", entry->d_name);
            // } else {
            //     syslog(LOG_ERR, "Failed to delete %s: %s", filepath, strerror(errno));
            // }
        }
    }

    closedir(dir);
    // syslog(LOG_INFO, "Finished cleanup of incident images directory");
}