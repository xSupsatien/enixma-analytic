/**
 * @file videoManager.js
 * @description Manages video stream setup and handling
 */

/**
 * Creates a video manager for handling video streams
 * @param {HTMLElement} videoElement - The video element to manage
 * @param {string} resolution - Desired video resolution
 * @returns {Object} Video manager interface
 */
export const createVideoManager = (videoElement, resolution) => {
    const originalSource = videoElement.src;

    const buildStreamUrl = () => {
        const protocol = window.location.protocol;
        const host = window.location.host;
        return `${protocol}//${host}/axis-cgi/mjpg/video.cgi?resolution=${resolution}`;
    };

    const setVideoSource = () => {
        const streamUrl = buildStreamUrl();
        const testImage = new Image();

        testImage.onload = () => {
            videoElement.src = streamUrl;
            // console.log('Stream URL loaded:', streamUrl);
        };

        testImage.onerror = () => {
            videoElement.src = originalSource;
            // console.log('Using fallback source:', originalSource);
        };

        testImage.src = streamUrl;
    };

    return {
        initialize: setVideoSource,
        getElement: () => videoElement
    };
};