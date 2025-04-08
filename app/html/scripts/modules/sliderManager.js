/**
 * @file sliderManager.js
 * @description Factory function to create slider managers for handling interactive slider inputs and confidence data
 */

/**
 * Creates a slider manager instance
 * @param {string} name - Identifier for the slider manager instance
 * @param {string} path - API endpoint path for confidence data
 * @returns {Object} Slider manager interface
 */
const createSliderManager = (name, path) => {
    // Internal state
    let values = [];
    let confidenceData = null;

    /**
     * Creates a slider configuration object
     * @param {HTMLElement} rangeEl - Range input element
     * @param {HTMLElement} textEl - Text input element
     * @returns {Object} Slider configuration object
     */
    function createSlider(rangeEl, textEl) {
        return {
            range: rangeEl,
            text: textEl,
            value: rangeEl.value,
            updateValue(val) {
                this.value = val;
                this.range.value = val;
                this.text.value = val;
                this.updateBackground();
            },
            updateBackground() {
                const progress = this.range.min == 0 ?
                    (this.value / this.range.max) * 100 :
                    ((100 / (this.range.max - this.range.min)) * this.value) -
                    (100 / (this.range.max - this.range.min));
                this.range.style.background = `linear-gradient(to right, #00fefc ${progress}%, #525252 ${progress}%)`;
            }
        };
    }

    /**
     * Sends confidence data to the server
     * @param {Array} data - Confidence data to send
     * @param {string} method - HTTP method to use
     */
    function sendToCgi(data, method = 'POST') {
        // console.log(`${name} slider to cgi: `, data);
        const originalMethod = method;

        $.ajax({
            url: path,
            type: 'GET'
        })
            .done(function (response) {
                console.log(`${name} slider data:`, response);

                let effectiveMethod = originalMethod;
                if (originalMethod === 'POST' && response?.data?.length) {
                    effectiveMethod = 'PUT';
                }

                $.ajax({
                    url: path,
                    type: effectiveMethod,
                    data: JSON.stringify(data),
                    contentType: 'application/json',
                    success: function (response, status) {
                        if (status === 'success') {
                            console.log('Success:', response);
                            if (response.error) {
                                console.error('Server error:', response.error);
                                return;
                            }
                            if (response.data) {
                                console.log('Processed data:', response.data);
                                confidenceData = response.data;
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
                console.error(`Failed to load ${name} slider data:`, {
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    error: errorThrown
                });
            });
    }

    /**
     * Validates key input for text fields
     * @param {Event} event - Keyboard event
     * @returns {boolean} Whether the key is valid
     */
    function isValidKey(event) {
        return /^[0-9]$/.test(event.key) ||
            event.key === 'Backspace' ||
            event.key === 'Delete' ||
            event.key === 'ArrowLeft' ||
            event.key === 'ArrowRight' ||
            event.key === '.';
    }

    /**
     * Sets up event listeners for a slider pair
     * @param {Object} slider - Slider configuration object
     */
    function setupSliderEvents(slider) {
        slider.range.addEventListener("input", (event) => {
            const value = event.target.value;
            slider.updateValue(value);

            const id = slider.range.id;
            // console.log("id:" + id);

            if (id === "lane1") {
                firstCrosslineManager.reset();
                firstCrosslineManager.setDefault();
            } else if (id === "lane2") {
                secondCrosslineManager.reset();
                secondCrosslineManager.setDefault();
            } if (name === 'confidence' && id === "confidence") {
                sendToCgi([{ value: Number(value) }]);
            } else if (name === 'ppm' && id === "pixel-per-meter") {
                sendToCgi([{ value: Number(value) }]);
            } else if (id === "incidents1") {
                window.toggleManager.sendIncidentsToCgi($("#area1-incidents"), true);
            } else if (id === "incidents2") {
                window.toggleManager.sendIncidentsToCgi($("#area2-incidents"), true);
            } else if (id === "overspeed1") {
                window.toggleManager.sendOverspeedToCgi($("#area1-overspeed"), true);
            } else if (id === "overspeed2") {
                window.toggleManager.sendOverspeedToCgi($("#area2-overspeed"), true);
            } else if (id.endsWith("pcu")) {
                window.toggleManager.sendPcuToCgi($("#pcu"), true);
            }
        });

        slider.text.addEventListener("keydown", (event) => {
            if (!isValidKey(event)) {
                event.preventDefault();
            }

            if (event.key === 'Enter') {
                const value = Math.max(0, Math.min(slider.range.max, slider.text.value || 0));
                slider.updateValue(value);

                if (slider.range.id === "lane1") {
                    firstCrosslineManager.reset();
                    firstCrosslineManager.setDefault();
                } else if (slider.range.id === "lane2") {
                    secondCrosslineManager.reset();
                    secondCrosslineManager.setDefault();
                } else if (name === "confidence" && slider.range.id === "confidence") {
                    sendToCgi([{ value: Number(value) }]);
                } else if (name === "ppm" && slider.range.id === "pixel-per-meter") {
                    sendToCgi([{ value: Number(value) }]);
                } else if (slider.range.id === "incidents1") {
                    window.toggleManager.sendIncidentsToCgi($("#area1-incidents"), true);
                } else if (slider.range.id === "incidents2") {
                    window.toggleManager.sendIncidentsToCgi($("#area2-incidents"), true);
                } else if (slider.range.id === "overspeed1") {
                    window.toggleManager.sendOverspeedToCgi($("#area1-overspeed"), true);
                } else if (slider.range.id === "overspeed2") {
                    window.toggleManager.sendOverspeedToCgi($("#area2-overspeed"), true);
                } else if (slider.range.id.endsWith("pcu")) {
                    window.toggleManager.sendPcuToCgi($("#pcu"), true);
                }
            }
        });
    }

    // Create manager interface
    const manager = {
        initialize(config) {
            // Create slider objects
            values = [
                createSlider(config.firstLaneRange, config.firstLaneText),
                createSlider(config.secondLaneRange, config.secondLaneText),
                createSlider(config.confidenceRange, config.confidenceText),
                createSlider(config.firstIncidentsRange, config.firstIncidentsText),
                createSlider(config.secondIncidentsRange, config.secondIncidentsText),
                createSlider(config.firstOverSpeedRange, config.firstOverSpeedText),
                createSlider(config.secondOverSpeedRange, config.secondOverSpeedText),
                createSlider(config.pcuCarRange, config.pcuCarText),
                createSlider(config.pcuBikeRange, config.pcuBikeText),
                createSlider(config.pcuTruckRange, config.pcuTruckText),
                createSlider(config.pcuBusRange, config.pcuBusText),
                createSlider(config.pcuTaxiRange, config.pcuTaxiText),
                createSlider(config.pcuPickupRange, config.pcuPickupText),
                createSlider(config.pcuTrailerRange, config.pcuTrailerText),
                // Add the pixel-per-meter slider if it exists
                config.pixelPerMeterRange && config.pixelPerMeterText ?
                    createSlider(config.pixelPerMeterRange, config.pixelPerMeterText) : null
            ].filter(Boolean); // Filter out null values if pixel-per-meter slider doesn't exist

            // Set up event listeners and initial state
            values.forEach(slider => {
                setupSliderEvents(slider);
                slider.updateBackground();
            });
        },
        reset() {
            values.forEach(slider => {
                slider.updateValue(slider.range.min);
            });
        },
        getValues() {
            return values.map(slider => slider.value);
        },
        setConfidence(value) {
            const confidenceSlider = values.find(s => s.range.id === "confidence");
            if (confidenceSlider) {
                confidenceSlider.updateValue(value);
            }
        },
        setPixelPerMeter(value) {
            const ppmSlider = values.find(s => s.range.id === "pixel-per-meter");
            if (ppmSlider) {
                ppmSlider.updateValue(value);
            }
        },
        sendToCgi
    };

    // Initialize with data from the server
    $.ajax({
        url: path,
        type: 'GET'
    })
        .done(function (response) {
            // console.log(`${name} slider data:`, response);

            if (response && response.data && Array.isArray(response.data)) {
                if (name === 'confidence' && response.data[0]?.value !== undefined) {
                    manager.setConfidence(response.data[0].value);
                } else if (name === 'ppm' && response.data[0]?.value !== undefined) {
                    manager.setPixelPerMeter(response.data[0].value);
                }
                manager.sendToCgi(response.data, 'PUT');
            }
        })
        .fail(function (jqXHR, textStatus, errorThrown) {
            console.error(`Failed to load ${name} slider data:`, {
                status: jqXHR.status,
                statusText: jqXHR.statusText,
                responseText: jqXHR.responseText,
                error: errorThrown
            });
        });

    return manager;
};

/**
 * Creates a double slider manager for speed limit range inputs
 * @param {string} name - Identifier for the slider (e.g., "area1-limitspeed", "area2-limitspeed")
 * @param {string} path - API endpoint path for sending data
 * @returns {Object} Double slider manager interface
 */
const createDoubleSliderManager = (name, path) => {
    // State
    let minSlider, maxSlider, minText, maxText, rangeElement;

    /**
     * Updates the visual range indicator
     */
    function updateRange() {
        if (!rangeElement) return;

        const minValue = parseInt(minSlider.value);
        const maxValue = parseInt(maxSlider.value);
        const percent1 = ((minValue - parseInt(minSlider.min)) / (parseInt(minSlider.max) - parseInt(minSlider.min))) * 100;
        const percent2 = ((maxValue - parseInt(minSlider.min)) / (parseInt(minSlider.max) - parseInt(minSlider.min))) * 100;

        rangeElement.style.left = percent1 + '%';
        rangeElement.style.width = (percent2 - percent1) + '%';
    }

    /**
     * Initializes the double slider
     * @param {string} sliderId - Base ID for the slider (without -min/-max suffix)
     */
    function initialize(sliderId) {
        // Get DOM elements
        minSlider = document.getElementById(`${sliderId}-min`);
        maxSlider = document.getElementById(`${sliderId}-max`);
        minText = document.getElementById(`${sliderId}-min-text`);
        maxText = document.getElementById(`${sliderId}-max-text`);

        // Get the range indicator element
        rangeElement = minSlider.parentElement.querySelector('.range');

        // Update range visually
        updateRange();

        // Add event listeners for sliders
        minSlider.addEventListener('input', function () {
            // Ensure min doesn't exceed max
            const maxValue = parseInt(maxSlider.value);
            const newMinValue = Math.min(parseInt(this.value), maxValue - 1);
            this.value = newMinValue;
            minText.value = newMinValue;
            updateRange();
            window.toggleManager.sendLimitspeedToCgi($(`#${sliderId}`), true);
        });

        maxSlider.addEventListener('input', function () {
            // Ensure max doesn't go below min
            const minValue = parseInt(minSlider.value);
            const newMaxValue = Math.max(parseInt(this.value), minValue + 1);
            this.value = newMaxValue;
            maxText.value = newMaxValue;
            updateRange();
            window.toggleManager.sendLimitspeedToCgi($(`#${sliderId}`), true);
        });

        // Add event listeners for text inputs
        minText.addEventListener('keydown', isValidKey);
        maxText.addEventListener('keydown', isValidKey);

        minText.addEventListener('change', function () {
            const newVal = parseInt(this.value) || 0;
            const maxValue = parseInt(maxSlider.value);
            const newMinValue = Math.max(parseInt(minSlider.min), Math.min(newVal, maxValue - 1));
            this.value = newMinValue;
            minSlider.value = newMinValue;
            updateRange();
            window.toggleManager.sendLimitspeedToCgi($(`#${sliderId}`), true);
        });

        maxText.addEventListener('change', function () {
            const newVal = parseInt(this.value) || 0;
            const minValue = parseInt(minSlider.value);
            const newMaxValue = Math.max(minValue + 1, Math.min(newVal, parseInt(maxSlider.max)));
            this.value = newMaxValue;
            maxSlider.value = newMaxValue;
            updateRange();
            window.toggleManager.sendLimitspeedToCgi($(`#${sliderId}`), true);
        });

        // Enter key handlers
        minText.addEventListener('keydown', function (e) {
            if (e.key === 'Enter') {
                this.blur(); // Trigger the change event
            }
        });

        maxText.addEventListener('keydown', function (e) {
            if (e.key === 'Enter') {
                this.blur(); // Trigger the change event
            }
        });
    }

    /**
     * Validates key input for text fields
     * @param {Event} event - Keyboard event
     * @returns {boolean} Whether the key is valid
     */
    function isValidKey(event) {
        if (!/^[0-9]$/.test(event.key) &&
            event.key !== 'Backspace' &&
            event.key !== 'Delete' &&
            event.key !== 'ArrowLeft' &&
            event.key !== 'ArrowRight' &&
            event.key !== 'Tab' &&
            event.key !== 'Enter') {
            event.preventDefault();
            return false;
        }
        return true;
    }

    /**
     * Sets both min and max values
     * @param {number} min - Minimum value
     * @param {number} max - Maximum value
     */
    function setValues(min, max) {
        if (minSlider && maxSlider) {
            minSlider.value = min;
            maxSlider.value = max;
            minText.value = min;
            maxText.value = max;
            updateRange();
        }
    }

    /**
     * Gets current slider values
     * @returns {Object} Object containing min and max values
     */
    function getValues() {
        return {
            min: parseInt(minSlider.value),
            max: parseInt(maxSlider.value)
        };
    }

    // Interface
    return {
        initialize,
        setValues,
        getValues,
        updateRange
    };
};

// Export the factory function
export { createSliderManager, createDoubleSliderManager };

// Initialize slider manager if running in browser context
if (typeof window !== 'undefined' && window.config) {
    const confidenceSlider = createSliderManager('confidence', window.config.paths.confidence);
    const ppmSlider = createSliderManager('ppm', window.config.paths.ppm);

    // Export only the confidence slider manager
    window.confidenceSlider = confidenceSlider;
    window.ppmSlider = ppmSlider;
}