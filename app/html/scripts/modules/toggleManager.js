/**
 * @file toggleManager.js
 * @description Factory function to create toggle managers for handling toggle inputs and incident data
 */

/**
 * Creates a toggle manager instance
 * @param {string} name - Identifier for the toggle manager instance
 * @param {Object} paths - API endpoint paths for different types of toggle data
 * @returns {Object} Toggle manager interface
 */
const createToggleManager = (name, paths) => {
    // Internal state
    let toggles = {};
    let toggleValues = {};

    /**
     * Updates the UI based on toggle state
     * @param {string} toggleId - The ID of the toggle element (without #)
     * @param {boolean} state - The state to set the toggle to
     */
    function updateUI(toggleId, state) {
        const $toggle = $(`#${toggleId}`);
        const area = toggleId.startsWith('area1') ? 'area1' : 'area2';

        // Set toggle state
        $toggle.prop('checked', state);

        // Handle special cases based on toggle type
        if (toggleId.endsWith('incidents-toggly')) {
            // Update incidents container
            $(`#${area}-incidents`).css({
                "border-style": state ? "solid" : "dashed",
                "border-color": state ? "#00fefc" : "rgb(122, 122, 122)"
            });

            // Show/hide tuning section
            $(`#${area}-incidents-tuning`).css("display", state ? "block" : "none");
            $(`#${area}-incidents-status-dot`).css("display", state ? "flex" : "none");
        }
        else if (toggleId.endsWith('wrongway-toggly')) {
            // Update wrongway container
            $(`#${area}-wrongway`).css({
                "border-style": state ? "solid" : "dashed",
                "border-color": state ? "#00fefc" : "rgb(122, 122, 122)"
            });

            $(`#${area}-wrongway-status-dot`).css("display", state ? "flex" : "none");
        }
        else if (toggleId.endsWith('truckright-toggly')) {
            // Update truckright container
            $(`#${area}-truckright`).css({
                "border-style": state ? "solid" : "dashed",
                "border-color": state ? "#00fefc" : "rgb(122, 122, 122)"
            });

            $(`#${area}-truckright-status-dot`).css("display", state ? "flex" : "none");
        }
        else if (toggleId.endsWith('overspeed-toggly')) {
            // Update overspeed container
            $(`#${area}-overspeed`).css({
                "border-style": state ? "solid" : "dashed",
                "border-color": state ? "#00fefc" : "rgb(122, 122, 122)"
            });

            // Show/hide tuning section
            $(`#${area}-overspeed-tuning`).css("display", state ? "block" : "none");
            $(`#${area}-overspeed-status-dot`).css("display", state ? "flex" : "none");
        }
        else if (toggleId.endsWith('limitspeed-toggly')) {
            // Update limitspeed container
            $(`#${area}-limitspeed`).css({
                "border-style": state ? "solid" : "dashed",
                "border-color": state ? "#00fefc" : "rgb(122, 122, 122)"
            });

            // Show/hide tuning section
            $(`#${area}-limitspeed-tuning`).css("display", state ? "block" : "none");
            $(`#${area}-limitspeed-status-dot`).css("display", state ? "flex" : "none");
        }

        // For incidents sub-toggles, update the individual properties
        if (['accident', 'broken', 'stop', 'block', 'construction'].some(type => toggleId.includes(type))) {
            const type = toggleId.split('-')[1]; // Extract type from ID

            // Make sure the array exists and has at least one item
            if (!toggleValues[`${area}-incidents`] || !Array.isArray(toggleValues[`${area}-incidents`])) {
                toggleValues[`${area}-incidents`] = [{}];
            }

            // Update the property in the first object of the array
            toggleValues[`${area}-incidents`][0][type] = state;
        }
        // For main incidents toggle, create proper structure
        else if (toggleId.endsWith('incidents-toggly')) {
            if (!state) {
                toggleValues[`${area}-incidents`] = [];
            } else if (!toggleValues[`${area}-incidents`] || !Array.isArray(toggleValues[`${area}-incidents`]) || toggleValues[`${area}-incidents`].length === 0) {
                // Initialize with defaults when turning on
                toggleValues[`${area}-incidents`] = [{
                    timer: 30,
                    accident: false,
                    broken: false,
                    stop: false,
                    block: false,
                    construction: false
                }];
            }
        }
        // For wrongway toggle
        else if (toggleId.endsWith('wrongway-toggly')) {
            toggleValues[`${area}-wrongway`] = [{ value: state }];
        }
        // For truckright toggle
        else if (toggleId.endsWith('truckright-toggly')) {
            toggleValues[`${area}-truckright`] = [{ value: state }];
        }
        // For overspeed toggle
        else if (toggleId.endsWith('overspeed-toggly')) {
            if (!state) {
                toggleValues[`${area}-overspeed`] = [];
            } else if (!toggleValues[`${area}-overspeed`] || !Array.isArray(toggleValues[`${area}-overspeed`]) || toggleValues[`${area}-overspeed`].length === 0) {
                // Initialize with defaults when turning on
                toggleValues[`${area}-overspeed`] = [{ timer: 120 }];
            }
        }
        // For limitspeed toggle
        else if (toggleId.endsWith('limitspeed-toggly')) {
            if (!state) {
                toggleValues[`${area}-limitspeed`] = [];
            } else if (!toggleValues[`${area}-limitspeed`] || !Array.isArray(toggleValues[`${area}-limitspeed`]) || toggleValues[`${area}-limitspeed`].length === 0) {
                // Initialize with defaults when turning on
                toggleValues[`${area}-limitspeed`] = [{
                    min: 90,
                    max: 120
                }];
            }
        }
    }

    /**
     * Handles toggle event for incidents and wrong way toggles
     * @param {jQuery} $toggle - jQuery element for the toggle
     * @param {boolean} isChecked - Whether the toggle is checked
     */
    function handleToggleEvent($toggle, isChecked) {
        if ($toggle.is("#area1-incidents-toggly")) {
            sendIncidentsToCgi($("#area1-incidents"), isChecked);
        }
        else if ($toggle.is("#area1-accident-toggly, #area1-broken-toggly, #area1-stop-toggly, #area1-block-toggly, #area1-construction-toggly")) {
            sendIncidentsToCgi($("#area1-incidents"), true);
        }
        else if ($toggle.is("#area2-incidents-toggly")) {
            sendIncidentsToCgi($("#area2-incidents"), isChecked);
        }
        else if ($toggle.is("#area2-accident-toggly, #area2-broken-toggly, #area2-stop-toggly, #area2-block-toggly, #area2-construction-toggly")) {
            sendIncidentsToCgi($("#area2-incidents"), true);
        }
        else if ($toggle.is("#area1-wrongway-toggly, #area2-wrongway-toggly")) {
            sendWrongwayToCgi($toggle.is("#area1-wrongway-toggly") ? $("#area1-wrongway") : $("#area2-wrongway"), isChecked);
        }
        else if ($toggle.is("#area1-truckright-toggly, #area2-truckright-toggly")) {
            sendTruckrightToCgi($toggle.is("#area1-truckright-toggly") ? $("#area1-truckright") : $("#area2-truckright"), isChecked);
        }
        else if ($toggle.is("#area1-overspeed-toggly, #area2-overspeed-toggly")) {
            sendOverspeedToCgi($toggle.is("#area1-overspeed-toggly") ? $("#area1-overspeed") : $("#area2-overspeed"), isChecked);
        }
        else if ($toggle.is("#area1-limitspeed-toggly, #area2-limitspeed-toggly")) {
            sendLimitspeedToCgi($toggle.is("#area1-limitspeed-toggly") ? $("#area1-limitspeed") : $("#area2-limitspeed"), isChecked);
        }
        else if ($toggle.is("#pcu-toggly, #display-pcu-toggly")) {
            sendPcuToCgi($("#pcu"), isChecked);
        }
    }

    /**
     * Sends incidents data to the server
     * @param {jQuery} incidentsElement - Incidents element jQuery object
     * @param {boolean} incidentsValue - Whether incidents are enabled
     */
    function sendIncidentsToCgi(incidentsElement, incidentsValue) {
        // console.log(incidentsElement.attr("id") + " incidents: ", incidentsValue);
        incidentsElement.css({
            "border-style": incidentsValue ? "solid" : "dashed",
            "border-color": incidentsValue ? "#00fefc" : "rgb(122, 122, 122)"
        });

        let path, key, incidents_slider, incidents_value, incidentsTuningId, incidentsTogglyId, incidentsStatusDotId, accidentTogglyId, brokenTogglyId, stopTogglyId, blockTogglyId, constructionTogglyId;
        if (incidentsElement.attr("id") === "area1-incidents") {
            path = paths.firstIncidents;
            key = "area1-incidents";
            incidents_slider = document.querySelector('input#incidents1[type="range"]');
            incidents_value = document.querySelector('input#incidents1[type="text"]');
            incidentsTuningId = "#area1-incidents-tuning";
            incidentsTogglyId = "#area1-incidents-toggly";
            incidentsStatusDotId = "#area1-incidents-status-dot";

            accidentTogglyId = "#area1-accident-toggly";
            brokenTogglyId = "#area1-broken-toggly";
            stopTogglyId = "#area1-stop-toggly";
            blockTogglyId = "#area1-block-toggly";
            constructionTogglyId = "#area1-construction-toggly";
        } else {
            path = paths.secondIncidents;
            key = "area2-incidents";
            incidents_slider = document.querySelector('input#incidents2[type="range"]');
            incidents_value = document.querySelector('input#incidents2[type="text"]');
            incidentsTuningId = "#area2-incidents-tuning";
            incidentsTogglyId = "#area2-incidents-toggly";
            incidentsStatusDotId = "#area2-incidents-status-dot";

            accidentTogglyId = "#area2-accident-toggly";
            brokenTogglyId = "#area2-broken-toggly";
            stopTogglyId = "#area2-stop-toggly";
            blockTogglyId = "#area2-block-toggly";
            constructionTogglyId = "#area2-construction-toggly";
        }

        var incidentsTuning = $(incidentsTuningId);
        incidentsTuning.css("display", incidentsValue ? "block" : "none");

        var incidentsStatusDot = $(incidentsStatusDotId);
        incidentsStatusDot.css("display", incidentsValue ? "flex" : "none");

        var incidentsToggly = $(incidentsTogglyId);
        incidentsToggly.prop("checked", incidentsValue);

        var value = [];

        if (incidentsValue) {
            value = [{
                timer: incidents_slider ? Number(incidents_slider.value) : 30,
                accident: $(accidentTogglyId).prop("checked"),
                broken: $(brokenTogglyId).prop("checked"),
                stop: $(stopTogglyId).prop("checked"),
                block: $(blockTogglyId).prop("checked"),
                construction: $(constructionTogglyId).prop("checked")
            }];
        }

        // console.log(key, 'to cgi: ', value);

        // Use AJAX approach
        $.ajax({
            url: path,
            type: 'GET'
        })
            .done(function (response) {
                // console.log(`${key} incidents data:`, response);

                let effectiveMethod = 'POST';
                if (response?.data?.length) {
                    effectiveMethod = 'PUT';
                }

                $.ajax({
                    url: path,
                    type: effectiveMethod,
                    data: JSON.stringify(value),
                    contentType: 'application/json',
                    success: function (response, status) {
                        if (status === 'success') {
                            console.log('Success:', response);
                            if (response.data) {
                                console.log('Processed data:', response.data);
                                // Update local state
                                toggleValues[key] = value;
                            }
                        }
                    },
                    error: function (jqXHR, textStatus, errorThrown) {
                        console.error('Request failed:', {
                            status: jqXHR.status,
                            statusText: jqXHR.statusText,
                            responseText: jqXHR.responseText,
                            error: errorThrown
                        });
                    }
                });
            })
            .fail(function (jqXHR, textStatus, errorThrown) {
                console.error(`Failed to load ${key} incidents data:`, {
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    error: errorThrown
                });
            });
    }

    /**
     * Sends wrong way data to the server
     * @param {jQuery} wrongwayElement - Wrong way element jQuery object
     * @param {boolean} wrongwayValue - Whether wrong way is enabled
     */
    function sendWrongwayToCgi(wrongwayElement, wrongwayValue) {
        // console.log(wrongwayElement.attr("id") + " incidents: ", wrongwayValue);
        wrongwayElement.css({
            "border-style": wrongwayValue ? "solid" : "dashed",
            "border-color": wrongwayValue ? "#00fefc" : "rgb(122, 122, 122)"
        });

        let path, key, wrongWayTogglyId, wrongWayStatusDotId;
        if (wrongwayElement.attr("id") === "area1-wrongway") {
            path = paths.firstWrongWay;
            key = "area1-wrongway";
            wrongWayTogglyId = "#area1-wrongway-toggly";
            wrongWayStatusDotId = "#area1-wrongway-status-dot";
        } else {
            path = paths.secondWrongWay;
            key = "area2-wrongway";
            wrongWayTogglyId = "#area2-wrongway-toggly";
            wrongWayStatusDotId = "#area2-wrongway-status-dot";
        }

        var wrongWayStatusDot = $(wrongWayStatusDotId);
        wrongWayStatusDot.css("display", wrongwayValue ? "flex" : "none");

        var wrongWayToggly = $(wrongWayTogglyId);
        wrongWayToggly.prop("checked", wrongwayValue);

        var value = [{
            value: wrongwayValue
        }];
        // console.log(key, 'to cgi: ', value);

        // Use AJAX approach
        $.ajax({
            url: path,
            type: 'GET'
        })
            .done(function (response) {
                // console.log(`${key} wrongway data:`, response);

                let effectiveMethod = 'POST';
                if (response?.data?.length) {
                    effectiveMethod = 'PUT';
                }

                $.ajax({
                    url: path,
                    type: effectiveMethod,
                    data: JSON.stringify(value),
                    contentType: 'application/json',
                    success: function (response, status) {
                        if (status === 'success') {
                            console.log('Success:', response);
                            if (response.data) {
                                console.log('Processed data:', response.data);
                                // Update local state
                                toggleValues[key] = value;
                            }
                        }
                    },
                    error: function (jqXHR, textStatus, errorThrown) {
                        console.error('Request failed:', {
                            status: jqXHR.status,
                            statusText: jqXHR.statusText,
                            responseText: jqXHR.responseText,
                            error: errorThrown
                        });
                    }
                });
            })
            .fail(function (jqXHR, textStatus, errorThrown) {
                console.error(`Failed to load ${key} wrongway data:`, {
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    error: errorThrown
                });
            });
    }

    /**
     * Sends truck right data to the server
     * @param {jQuery} truckrightElement - Truck right element jQuery object
     * @param {boolean} truckrightValue - Whether truck right is enabled
     */
    function sendTruckrightToCgi(truckrightElement, truckrightValue) {
        // console.log(truckrightElement.attr("id") + " incidents: ", truckrightValue);
        truckrightElement.css({
            "border-style": truckrightValue ? "solid" : "dashed",
            "border-color": truckrightValue ? "#00fefc" : "rgb(122, 122, 122)"
        });

        let path, key, truckrightTogglyId, truckrightStatusDotId;
        if (truckrightElement.attr("id") === "area1-truckright") {
            path = paths.firstTruckRight;
            key = "area1-truckright";
            truckrightTogglyId = "#area1-truckright-toggly";
            truckrightStatusDotId = "#area1-truckright-status-dot";
        } else {
            path = paths.secondTruckRight;
            key = "area2-truckright";
            truckrightTogglyId = "#area2-truckright-toggly";
            truckrightStatusDotId = "#area2-truckright-status-dot";
        }

        var truckrightStatusDot = $(truckrightStatusDotId);
        truckrightStatusDot.css("display", truckrightValue ? "flex" : "none");

        var truckrightToggly = $(truckrightTogglyId);
        truckrightToggly.prop("checked", truckrightValue);

        var value = [{
            value: truckrightValue
        }];
        // console.log(key, 'to cgi: ', value);

        // Use AJAX approach
        $.ajax({
            url: path,
            type: 'GET'
        })
            .done(function (response) {
                // console.log(`${key} truckright data:`, response);

                let effectiveMethod = 'POST';
                if (response?.data?.length) {
                    effectiveMethod = 'PUT';
                }

                $.ajax({
                    url: path,
                    type: effectiveMethod,
                    data: JSON.stringify(value),
                    contentType: 'application/json',
                    success: function (response, status) {
                        if (status === 'success') {
                            console.log('Success:', response);
                            if (response.data) {
                                console.log('Processed data:', response.data);
                                // Update local state
                                toggleValues[key] = value;
                            }
                        }
                    },
                    error: function (jqXHR, textStatus, errorThrown) {
                        console.error('Request failed:', {
                            status: jqXHR.status,
                            statusText: jqXHR.statusText,
                            responseText: jqXHR.responseText,
                            error: errorThrown
                        });
                    }
                });
            })
            .fail(function (jqXHR, textStatus, errorThrown) {
                console.error(`Failed to load ${key} truckright data:`, {
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    error: errorThrown
                });
            });
    }

    /**
     * Sends over speed data to the server
     * @param {jQuery} overspeedElement - Over speed element jQuery object
     * @param {boolean} overspeedValue - Whether over speed is enabled
     */
    function sendOverspeedToCgi(overspeedElement, overspeedValue) {
        // console.log(overspeedElement.attr("id") + " incidents: ", overspeedValue);
        overspeedElement.css({
            "border-style": overspeedValue ? "solid" : "dashed",
            "border-color": overspeedValue ? "#00fefc" : "rgb(122, 122, 122)"
        });

        let path, key, overspeedSlider, overspeedTuningId, overspeedTogglyId, overspeedStatusDotId;
        if (overspeedElement.attr("id") === "area1-overspeed") {
            path = paths.firstOverSpeed;
            key = "area1-overspeed";
            overspeedSlider = document.querySelector('input#overspeed1[type="range"]');
            overspeedTuningId = "#area1-overspeed-tuning";
            overspeedTogglyId = "#area1-overspeed-toggly";
            overspeedStatusDotId = "#area1-overspeed-status-dot";
        } else {
            path = paths.secondOverSpeed;
            key = "area2-overspeed";
            overspeedSlider = document.querySelector('input#overspeed2[type="range"]');
            overspeedTuningId = "#area2-overspeed-tuning";
            overspeedTogglyId = "#area2-overspeed-toggly";
            overspeedStatusDotId = "#area2-overspeed-status-dot";
        }

        var overspeedTuning = $(overspeedTuningId);
        overspeedTuning.css("display", overspeedValue ? "block" : "none");

        var overspeedStatusDot = $(overspeedStatusDotId);
        overspeedStatusDot.css("display", overspeedValue ? "flex" : "none");

        var overspeedToggly = $(overspeedTogglyId);
        overspeedToggly.prop("checked", overspeedValue);

        var value = [];

        if (overspeedValue) {
            value = [{
                value: Number(overspeedSlider.value)
            }];
        }

        // console.log(key, 'to cgi: ', value);

        // Use AJAX approach
        $.ajax({
            url: path,
            type: 'GET'
        })
            .done(function (response) {
                // console.log(`${key} overspeed data:`, response);

                let effectiveMethod = 'POST';
                if (response?.data?.length) {
                    effectiveMethod = 'PUT';
                }

                $.ajax({
                    url: path,
                    type: effectiveMethod,
                    data: JSON.stringify(value),
                    contentType: 'application/json',
                    success: function (response, status) {
                        if (status === 'success') {
                            console.log('Success:', response);
                            if (response.data) {
                                console.log('Processed data:', response.data);
                                // Update local state
                                toggleValues[key] = value;
                            }
                        }
                    },
                    error: function (jqXHR, textStatus, errorThrown) {
                        console.error('Request failed:', {
                            status: jqXHR.status,
                            statusText: jqXHR.statusText,
                            responseText: jqXHR.responseText,
                            error: errorThrown
                        });
                    }
                });
            })
            .fail(function (jqXHR, textStatus, errorThrown) {
                console.error(`Failed to load ${key} overspeed data:`, {
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    error: errorThrown
                });
            });
    }

    /**
     * Sends limit speed data to the server
     * @param {jQuery} limitspeedElement - Limit speed element jQuery object
     * @param {boolean} limitspeedValue - Whether limit speed is enabled
     */
    function sendLimitspeedToCgi(limitspeedElement, limitspeedValue) {
        // console.log(limitspeedElement.attr("id") + " incidents: ", limitspeedValue);
        limitspeedElement.css({
            "border-style": limitspeedValue ? "solid" : "dashed",
            "border-color": limitspeedValue ? "#00fefc" : "rgb(122, 122, 122)"
        });

        let path, key, minSlider, maxSlider, limitspeedTuningId, limitspeedTogglyId, limitspeedStatusDotId;
        if (limitspeedElement.attr("id") === "area1-limitspeed") {
            path = paths.firstLimitSpeed;
            key = "area1-limitspeed";
            minSlider = document.getElementById("area1-limitspeed-min");
            maxSlider = document.getElementById("area1-limitspeed-max");
            limitspeedTuningId = "#area1-limitspeed-tuning";
            limitspeedTogglyId = "#area1-limitspeed-toggly";
            limitspeedStatusDotId = "#area1-limitspeed-status-dot";
        } else {
            path = paths.secondLimitSpeed;
            key = "area2-limitspeed";
            minSlider = document.getElementById("area2-limitspeed-min");
            maxSlider = document.getElementById("area2-limitspeed-max");
            limitspeedTuningId = "#area2-limitspeed-tuning";
            limitspeedTogglyId = "#area2-limitspeed-toggly";
            limitspeedStatusDotId = "#area2-limitspeed-status-dot";
        }

        var limitspeedTuning = $(limitspeedTuningId);
        limitspeedTuning.css("display", limitspeedValue ? "block" : "none");

        var limitspeedStatusDot = $(limitspeedStatusDotId);
        limitspeedStatusDot.css("display", limitspeedValue ? "flex" : "none");

        var limitspeedToggly = $(limitspeedTogglyId);
        limitspeedToggly.prop("checked", limitspeedValue);

        var value = [];

        if (limitspeedValue) {
            value = [{
                min: Number(minSlider.value),
                max: Number(maxSlider.value)
            }];
        }

        // console.log(key, 'to cgi: ', value);

        // Use AJAX approach
        $.ajax({
            url: path,
            type: 'GET'
        })
            .done(function (response) {
                // console.log(`${key} limitspeed data:`, response);

                let effectiveMethod = 'POST';
                if (response?.data?.length) {
                    effectiveMethod = 'PUT';
                }

                $.ajax({
                    url: path,
                    type: effectiveMethod,
                    data: JSON.stringify(value),
                    contentType: 'application/json',
                    success: function (response, status) {
                        if (status === 'success') {
                            console.log('Success:', response);
                            if (response.data) {
                                console.log('Processed data:', response.data);
                                // Update local state
                                toggleValues[key] = value;
                            }
                        }
                    },
                    error: function (jqXHR, textStatus, errorThrown) {
                        console.error('Request failed:', {
                            status: jqXHR.status,
                            statusText: jqXHR.statusText,
                            responseText: jqXHR.responseText,
                            error: errorThrown
                        });
                    }
                });
            })
            .fail(function (jqXHR, textStatus, errorThrown) {
                console.error(`Failed to load ${key} limitspeed data:`, {
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    error: errorThrown
                });
            });
    }

    /**
     * Sends pcu data to the server
     * @param {jQuery} pcuElement - pcu element jQuery object
     * @param {boolean} pcuValue - Whether pcu are enabled
     */
    function sendPcuToCgi(pcuElement, pcuValue) {
        // console.log("pcu: ", pcuValue);
        pcuElement.css("display", pcuValue ? "block" : "none");

        var pcuStatusDot = $("#pcu-status-dot");
        pcuStatusDot.css("display", pcuValue ? "flex" : "none");

        $("#pcu-toggly, #display-pcu-toggly").prop('checked', pcuValue);

        var value = [];

        let path = paths.pcu;
        let key = "pcu";
        let pcu_car_slider = document.querySelector('input#car-pcu[type="range"]');
        let pcu_bike_slider = document.querySelector('input#bike-pcu[type="range"]');
        let pcu_truck_slider = document.querySelector('input#truck-pcu[type="range"]');
        let pcu_bus_slider = document.querySelector('input#bus-pcu[type="range"]');
        let pcu_taxi_slider = document.querySelector('input#taxi-pcu[type="range"]');
        let pcu_pickup_slider = document.querySelector('input#pickup-pcu[type="range"]');
        let pcu_trailer_slider = document.querySelector('input#trailer-pcu[type="range"]');

        if (pcuValue) {
            value = [{
                car_pcu: pcu_car_slider ? Number(pcu_car_slider.value) : 1,
                bike_pcu: pcu_bike_slider ? Number(pcu_bike_slider.value) : 0.25,
                truck_pcu: pcu_truck_slider ? Number(pcu_truck_slider.value) : 2.5,
                bus_pcu: pcu_bus_slider ? Number(pcu_bus_slider.value) : 2,
                taxi_pcu: pcu_taxi_slider ? Number(pcu_taxi_slider.value) : 1,
                pickup_pcu: pcu_pickup_slider ? Number(pcu_pickup_slider.value) : 1,
                trailer_pcu: pcu_trailer_slider ? Number(pcu_trailer_slider.value) : 2.5
            }];
        }

        // console.log('pcu to cgi: ', value);

        // Use AJAX approach
        $.ajax({
            url: path,
            type: 'GET'
        })
            .done(function (response) {
                // console.log(`${key} incidents data:`, response);

                let effectiveMethod = 'POST';
                if (response?.data?.length) {
                    effectiveMethod = 'PUT';
                }

                $.ajax({
                    url: path,
                    type: effectiveMethod,
                    data: JSON.stringify(value),
                    contentType: 'application/json',
                    success: function (response, status) {
                        if (status === 'success') {
                            console.log('Success:', response);
                            if (response.data) {
                                console.log('Processed data:', response.data);
                                // Update local state
                                toggleValues[key] = value;
                            }
                        }
                    },
                    error: function (jqXHR, textStatus, errorThrown) {
                        console.error('Request failed:', {
                            status: jqXHR.status,
                            statusText: jqXHR.statusText,
                            responseText: jqXHR.responseText,
                            error: errorThrown
                        });
                    }
                });
            })
            .fail(function (jqXHR, textStatus, errorThrown) {
                console.error(`Failed to load ${key} incidents data:`, {
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    error: errorThrown
                });
            });
    }

    // Create manager interface
    const manager = {
        /**
         * Initialize the toggle manager
         * @returns {void}
         */
        initialize() {
            // Create global compatibility functions
            window.send_incidents2cgi = function (incidentsElement, incidentsValue) {
                sendIncidentsToCgi(incidentsElement, incidentsValue);
            };

            window.send_wrongway2cgi = function (wrongwayElement, wrongwayValue) {
                sendWrongwayToCgi(wrongwayElement, wrongwayValue);
            };

            window.send_truckright2cgi = function (truckrightElement, truckrightValue) {
                sendTruckrightToCgi(truckrightElement, truckrightValue);
            };

            window.send_overspeed2cgi = function (overspeedElement, overspeedValue) {
                sendOverspeedToCgi(overspeedElement, overspeedValue);
            };

            window.send_limitspeed2cgi = function (limitspeedElement, limitspeedValue) {
                sendLimitspeedToCgi(limitspeedElement, limitspeedValue);
            };

            window.send_pcu2cgi = function (pcuElement, pcuValue) {
                sendPcuToCgi(pcuElement, pcuValue);
            };

            // List of toggle selectors
            const toggleSelectors = [
                "#area1-incidents-toggly", "#area2-incidents-toggly",
                "#area1-accident-toggly", "#area2-accident-toggly",
                "#area1-broken-toggly", "#area2-broken-toggly",
                "#area1-stop-toggly", "#area2-stop-toggly",
                "#area1-block-toggly", "#area2-block-toggly",
                "#area1-construction-toggly", "#area2-construction-toggly",
                "#area1-wrongway-toggly", "#area2-wrongway-toggly",
                "#area1-truckright-toggly", "#area2-truckright-toggly",
                "#area1-overspeed-toggly", "#area2-overspeed-toggly",
                "#area1-limitspeed-toggly", "#area2-limitspeed-toggly",
                "#pcu-toggly", "#display-pcu-toggly"
            ];

            // Store references to toggle elements
            toggleSelectors.forEach(selector => {
                toggles[selector] = $(selector);

                // Set up event listeners
                $(selector).on("click", function () {
                    const isChecked = $(this).prop("checked");
                    handleToggleEvent($(this), isChecked);
                });
            });

            // Initialize with data from the server
            this.loadInitialState();
        },

        /**
         * Load the initial state from the server
         */
        loadInitialState() {
            // Load incidents data
            $.ajax({
                url: paths.firstIncidents,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const incidentsData = response.data;
                        // Update UI based on server data
                        manager.setIncidentsState("area1", incidentsData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area1 incidents data:", err);
                });

            $.ajax({
                url: paths.secondIncidents,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const incidentsData = response.data;
                        // Update UI based on server data
                        manager.setIncidentsState("area2", incidentsData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area2 incidents data:", err);
                });

            // Load wrong way data
            $.ajax({
                url: paths.firstWrongWay,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const wrongwayData = response.data;
                        // Update UI based on server data
                        manager.setWrongwayState("area1", wrongwayData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area1 wrongway data:", err);
                });

            $.ajax({
                url: paths.secondWrongWay,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const wrongwayData = response.data;
                        // Update UI based on server data
                        manager.setWrongwayState("area2", wrongwayData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area2 wrongway data:", err);
                });

            // Load truck right data
            $.ajax({
                url: paths.firstTruckRight,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const truckrightData = response.data;
                        // Update UI based on server data
                        manager.setTruckrightState("area1", truckrightData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area1 truckright data:", err);
                });

            $.ajax({
                url: paths.secondTruckRight,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const truckrightData = response.data;
                        // Update UI based on server data
                        manager.setTruckrightState("area2", truckrightData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area2 truckright data:", err);
                });

            // Load over speed data
            $.ajax({
                url: paths.firstOverSpeed,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const overspeedData = response.data;
                        // Update UI based on server data
                        manager.setOverspeedState("area1", overspeedData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area1 overspeed data:", err);
                });

            $.ajax({
                url: paths.secondOverSpeed,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const overspeedData = response.data;
                        // Update UI based on server data
                        manager.setOverspeedState("area2", overspeedData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area2 overspeed data:", err);
                });

            // Load over speed data
            $.ajax({
                url: paths.firstLimitSpeed,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const limitspeedData = response.data;
                        // Update UI based on server data
                        manager.setLimitspeedState("area1", limitspeedData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area1 limitspeed data:", err);
                });

            $.ajax({
                url: paths.secondLimitSpeed,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const limitspeedData = response.data;
                        // Update UI based on server data
                        manager.setLimitspeedState("area2", limitspeedData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area2 limitspeed data:", err);
                });

            // Load pcu data
            $.ajax({
                url: paths.pcu,
                type: 'GET'
            })
                .done(function (response) {
                    if (response && response.data) {
                        const pcuData = response.data;
                        // Update UI based on server data
                        manager.setPcuState(pcuData);
                    }
                })
                .fail(function (err) {
                    console.error("Failed to load area1 pcu data:", err);
                });
        },

        /**
         * Set the state of incidents toggles from loaded data
         * @param {string} area - The area identifier ("area1" or "area2")
         * @param {Array} data - The incidents data from server
         */
        setIncidentsState(area, data) {
            if (!data || !Array.isArray(data) || data.length === 0) return;

            // Save to internal state first
            toggleValues[`${area}-incidents`] = data;

            // Get the first item from the array
            const incident = data[0];
            const hasIncidents = Object.keys(incident).length > 0;

            // Update main incidents toggle UI
            updateUI(`${area}-incidents-toggly`, hasIncidents);

            if (hasIncidents) {
                // Set individual toggles if they exist in the data
                if (incident.accident !== undefined) updateUI(`${area}-accident-toggly`, incident.accident);
                if (incident.broken !== undefined) updateUI(`${area}-broken-toggly`, incident.broken);
                if (incident.stop !== undefined) updateUI(`${area}-stop-toggly`, incident.stop);
                if (incident.block !== undefined) updateUI(`${area}-block-toggly`, incident.block);
                if (incident.construction !== undefined) updateUI(`${area}-construction-toggly`, incident.construction);

                // Set timer if it exists
                if (incident.timer !== undefined) {
                    const incidents_slider = document.querySelector(`input#incidents${area.slice(-1)}[type="range"]`);
                    const incidents_value = document.querySelector(`input#incidents${area.slice(-1)}[type="text"]`);

                    if (incidents_slider) {
                        incidents_slider.value = incident.timer;
                        // Update slider background if it has that functionality
                        if (typeof incidents_slider.style !== 'undefined') {
                            const progress = ((100 / incidents_slider.max) * incident.timer);
                            incidents_slider.style.background = `linear-gradient(to right, #00fefc ${progress}%, #525252 ${progress}%)`;
                        }
                    }
                    if (incidents_value) incidents_value.value = incident.timer;
                }
            }
        },

        /**
         * Set the state of wrong way toggles from loaded data
         * @param {string} area - The area identifier ("area1" or "area2")
         * @param {Array} data - The wrong way data from server
         */
        setWrongwayState(area, data) {
            if (!data || !Array.isArray(data) || data.length === 0) return;

            // Save to internal state
            toggleValues[`${area}-wrongway`] = data;

            // Get the first item from the array
            const wrongway = data[0];
            const hasWrongway = wrongway.value === true;

            // Update UI using the updateUI function
            updateUI(`${area}-wrongway-toggly`, hasWrongway);
        },

        /**
         * Set the state of truck right toggles from loaded data
         * @param {string} area - The area identifier ("area1" or "area2")
         * @param {Array} data - The truck right data from server
         */
        setTruckrightState(area, data) {
            if (!data || !Array.isArray(data) || data.length === 0) return;

            // Save to internal state
            toggleValues[`${area}-truckright`] = data;

            // Get the first item from the array
            const truckright = data[0];
            const hasTruckright = truckright.value === true;

            // Update UI using the updateUI function
            updateUI(`${area}-truckright-toggly`, hasTruckright);
        },

        /**
         * Set the state of over speed toggles from loaded data
         * @param {string} area - The area identifier ("area1" or "area2")
         * @param {Array} data - The over speed data from server
         */
        setOverspeedState(area, data) {
            if (!data || !Array.isArray(data) || data.length === 0) return;

            // Save to internal state
            toggleValues[`${area}-overspeed`] = data;

            // Get the first item from the array
            const overspeed = data[0];
            const hasOverspeed = Object.keys(overspeed).length > 0;

            // Update UI using the updateUI function
            updateUI(`${area}-overspeed-toggly`, hasOverspeed);

            if (hasOverspeed) {
                if (overspeed.value !== undefined) {
                    const overspeed_slider = document.querySelector(`input#overspeed${area.slice(-1)}[type="range"]`);
                    const overspeed_value = document.querySelector(`input#overspeed${area.slice(-1)}[type="text"]`);

                    if (overspeed_slider) {
                        overspeed_slider.value = overspeed.value;
                        // Update slider background if it has that functionality
                        if (typeof overspeed_slider.style !== 'undefined') {
                            const progress = ((100 / overspeed_slider.max) * overspeed.value);
                            overspeed_slider.style.background = `linear-gradient(to right, #00fefc ${progress}%, #525252 ${progress}%)`;
                        }
                    }
                    if (overspeed_value) overspeed_value.value = overspeed.value;
                }
            }
        },

        /**
         * Set the state of limit speed toggles from loaded data
         * @param {string} area - The area identifier ("area1" or "area2")
         * @param {Array} data - The limit speed data from server
         */
        setLimitspeedState(area, data) {
            if (!data || !Array.isArray(data) || data.length === 0) return;

            // Save to internal state
            toggleValues[`${area}-limitspeed`] = data;

            // Get the first item from the array
            const limitspeed = data[0];
            const hasLimitspeed = Object.keys(limitspeed).length > 0;

            // Update UI using the updateUI function
            updateUI(`${area}-limitspeed-toggly`, hasLimitspeed);

            if (hasLimitspeed) {
                if (limitspeed.min !== undefined && limitspeed.max !== undefined) {
                    // Get DOM elements
                    const minSlider = document.getElementById(`${area}-limitspeed-min`);
                    const maxSlider = document.getElementById(`${area}-limitspeed-max`);
                    const minText = document.getElementById(`${area}-limitspeed-min-text`);
                    const maxText = document.getElementById(`${area}-limitspeed-max-text`);

                    // Get the range indicator element
                    const rangeElement = minSlider.parentElement.querySelector('.range');

                    // Set initial values
                    const minValue = parseInt(limitspeed.min);
                    minSlider.value = minValue;
                    minText.value = minValue;

                    const maxValue = parseInt(limitspeed.max);
                    maxSlider.value = maxValue;
                    maxText.value = maxValue;

                    const percent1 = ((minValue - minSlider.min) / (minSlider.max - minSlider.min)) * 100;
                    const percent2 = ((maxValue - minSlider.min) / (minSlider.max - minSlider.min)) * 100;

                    rangeElement.style.left = percent1 + '%';
                    rangeElement.style.width = (percent2 - percent1) + '%';
                }
            }
        },

        /**
         * Set the state of pcu toggles from loaded data
         * @param {string} area - The area identifier ("area1" or "area2")
         * @param {Array} data - The pcu data from server
         */
        setPcuState(data) {
            if (!data || !Array.isArray(data) || data.length === 0) return;

            // Save to internal state first
            toggleValues["pcu"] = data;

            // Get the first item from the array
            const pcu = data[0];
            const hasPCU = Object.keys(pcu).length > 0;

            // Update main pcu toggle UI
            $("#pcu-toggly, #display-pcu-toggly").prop('checked', hasPCU);
            $("#pcu").css("display", hasPCU ? "block" : "none");
            $("#pcu-status-dot").css("display", hasPCU ? "flex" : "none");

            if (!hasPCU) return;

            // Define vehicle types to process
            const vehicleTypes = [
                'car', 'bike', 'truck', 'bus',
                'taxi', 'pickup', 'trailer'
            ];

            vehicleTypes.forEach(vehicle => {
                const pcuKey = `${vehicle}_pcu`;

                if (pcu[pcuKey] !== undefined) {
                    const slider = document.querySelector(`input#${vehicle}-pcu[type="range"]`);
                    const textInput = document.querySelector(`input#${vehicle}-pcu[type="text"]`);

                    // Update slider if it exists
                    if (slider) {
                        slider.value = pcu[pcuKey];

                        // Update slider background if available
                        if (typeof slider.style !== 'undefined') {
                            const progress = ((100 / slider.max) * pcu[pcuKey]);
                            slider.style.background = `linear-gradient(to right, #00fefc ${progress}%, #525252 ${progress}%)`;
                        }
                    }

                    // Update text input if it exists
                    if (textInput) {
                        textInput.value = pcu[pcuKey];
                    }
                }
            });
        },

        /**
         * Get the current state of all toggles
         * @returns {Object} The current toggle values
         */
        getValues() {
            return toggleValues;
        },

        // Expose API methods
        sendIncidentsToCgi,
        sendWrongwayToCgi,
        sendTruckrightToCgi,
        sendOverspeedToCgi,
        sendLimitspeedToCgi,
        sendPcuToCgi
    };

    return manager;
};

// Export the factory function
export { createToggleManager };

// Initialize toggle manager if running in browser context
if (typeof window !== 'undefined' && window.config) {
    const toggleManager = createToggleManager('incidents', {
        firstIncidents: window.config.paths.firstIncidents,
        secondIncidents: window.config.paths.secondIncidents,
        firstWrongWay: window.config.paths.firstWrongWay,
        secondWrongWay: window.config.paths.secondWrongWay,
        firstTruckRight: window.config.paths.firstTruckRight,
        secondTruckRight: window.config.paths.secondTruckRight,
        firstOverSpeed: window.config.paths.firstOverSpeed,
        secondOverSpeed: window.config.paths.secondOverSpeed,
        firstLimitSpeed: window.config.paths.firstLimitSpeed,
        secondLimitSpeed: window.config.paths.secondLimitSpeed,
        pcu: window.config.paths.pcu
    });

    // Export manager for external use if needed
    window.toggleManager = toggleManager;
}