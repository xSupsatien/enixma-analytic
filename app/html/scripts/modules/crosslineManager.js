/**
 * @file crosslineManager.js
 * @description Factory function to create crossline managers for handling interactive line drawing
 */

import { sharedState } from './sharedState.js';

/**
 * Creates a crossline manager instance
 * @param {string} name - Identifier for the crossline manager instance
 * @param {string} path - API endpoint path for crossline data
 * @returns {Object} Crossline manager interface
 */
export const createCrosslineManager = (name, path) => {
    // Initialize variables first before creating any objects
    let directionForward = true; // Default direction is forward
    let isDefault = false;
    let isDragging = false;
    let isDraggingLine = false;
    let activePoint = null;
    let lastMousePosition = null;
    let crosslines = [createCrossline([])];

    /**
     * Creates a point object
     * @param {number} x - X coordinate
     * @param {number} y - Y coordinate
     * @returns {Object} Point object
     */
    function createPoint(x, y) {
        return { x, y };
    }

    /**
     * Creates a crossline object with drawing and point management
     * @param {Array} crosslineCf - Initial points configuration
     * @returns {Object} Crossline object
     */
    function createCrossline(crosslineCf) {
        return {
            points: crosslineCf,
            direction: true, // Default direction is forward (true)
            addPoint(p) {
                this.points.push(createPoint(Math.round(p.x), Math.round(p.y)));
            },
            draw() {
                ctx.lineWidth = 3;
                ctx.strokeStyle = '#00fefc';
                ctx.beginPath();

                // Draw line between points
                if (this.points.length > 0) {
                    ctx.moveTo(this.points[0].x, this.points[0].y);
                    for (let i = 1; i < this.points.length; i++) {
                        ctx.lineTo(this.points[i].x, this.points[i].y);
                    }
                }

                // Draw points
                this.points.forEach(p => {
                    ctx.moveTo(p.x + 4, p.y);
                    ctx.arc(p.x, p.y, 5, 0, Math.PI * 2);
                    ctx.arc(p.x, p.y, 10, 0, Math.PI * 2);
                });

                ctx.stroke();
                this.drawDirection();
            },
            drawDirection() {
                if (this.points.length < 2) return false;

                const prefix = name === 'first' ? '1' : '2';

                // Log the current direction state for debugging
                // console.log(`Drawing with direction: ${this.direction ? 'forward' : 'reverse'}`);

                ctx.beginPath();

                const arrow_size = 15;

                for (let i = 0; i < this.points.length - 1; i++) {
                    // Draw arrow below the line
                    const mid_x = (this.points[i].x + this.points[i + 1].x) / 2;
                    const mid_y = (this.points[i].y + this.points[i + 1].y) / 2;

                    // Calculate the angle of the line segment
                    let dx = this.points[i + 1].x - this.points[i].x;
                    let dy = this.points[i + 1].y - this.points[i].y;

                    // Reverse the direction if needed
                    if (!this.direction) {
                        dx = -dx;
                        dy = -dy;
                    }

                    const angle = Math.atan2(dy, dx);

                    // Rotate the original V-shaped arrow to match line direction
                    // Original V points were at (-arrow_size, +arrow_size), (0, +arrow_size*2), and (+arrow_size, +arrow_size)
                    // relative to mid_x, mid_y

                    // Calculate rotated points
                    const left_x = mid_x + (-arrow_size * Math.cos(angle) - arrow_size * Math.sin(angle));
                    const left_y = mid_y + (-arrow_size * Math.sin(angle) + arrow_size * Math.cos(angle));

                    const center_x = mid_x + (0 * Math.cos(angle) - arrow_size * 2 * Math.sin(angle));
                    const center_y = mid_y + (0 * Math.sin(angle) + arrow_size * 2 * Math.cos(angle));

                    const right_x = mid_x + (arrow_size * Math.cos(angle) - arrow_size * Math.sin(angle));
                    const right_y = mid_y + (arrow_size * Math.sin(angle) + arrow_size * Math.cos(angle));

                    ctx.moveTo(left_x, left_y);
                    ctx.lineTo(center_x, center_y);
                    ctx.lineTo(right_x, right_y);
                }

                ctx.stroke();
            },
            closest(pos, dist = 8) {
                let i = 0, index = -1;
                dist *= dist;
                for (const p of this.points) {
                    const x = pos.x - p.x;
                    const y = pos.y - p.y;
                    const d2 = x * x + y * y;
                    if (d2 < dist) {
                        dist = d2;
                        index = i;
                    }
                    i++;
                }
                if (index > -1) { return [this.points[index], index] }
            },
            isPointNearLine(pos, threshold = 8) {
                if (this.points.length < 2) return false;

                const p1 = this.points[0];
                const p2 = this.points[this.points.length - 1];

                // Calculate the distance from point to line segment
                const A = pos.x - p1.x;
                const B = pos.y - p1.y;
                const C = p2.x - p1.x;
                const D = p2.y - p1.y;

                const dot = A * C + B * D;
                const len_sq = C * C + D * D;

                // If line segment has zero length, return false
                if (len_sq === 0) return false;

                let param = dot / len_sq;

                // Find nearest point on line segment
                let xx, yy;

                if (param < 0) {
                    xx = p1.x;
                    yy = p1.y;
                } else if (param > 1) {
                    xx = p2.x;
                    yy = p2.y;
                } else {
                    xx = p1.x + param * C;
                    yy = p1.y + param * D;
                }

                const dx = pos.x - xx;
                const dy = pos.y - yy;
                const distance = Math.sqrt(dx * dx + dy * dy);

                return distance <= threshold;
            },
            moveLine(dx, dy) {
                this.points.forEach(point => {
                    point.x += dx;
                    point.y += dy;
                });
            },
            // No longer needed as we're handling this directly in the click handler
            // Left for API compatibility
            toggleDirection() {
                this.direction = !this.direction;
                return this.direction;
            }
        };
    }

    /**
     * Updates crossline state based on mouse interaction
     * @param {Object} mouse - Mouse state data
     * @returns {Array|null} Active point data if any
     */
    function update(mouse) {
        // Always check mouse state first
        sharedState.checkMouseState(mouse);

        const currentCrossline = crosslines[0];

        // If no dragging is in progress, check for new interactions
        if (!isDragging && !isDraggingLine) {
            activePoint = currentCrossline.closest(mouse);

            if (mouse.button) {
                if (activePoint) {
                    sharedState.setActiveManager('crossline');
                    isDragging = true;
                } else if (currentCrossline.isPointNearLine(mouse)) {
                    isDraggingLine = true;
                    lastMousePosition = { x: mouse.x, y: mouse.y };
                    sharedState.setActiveManager('crossline');
                }
            }
        }

        // Handle ongoing dragging
        if (isDraggingLine) {
            if (mouse.button) {
                const dx = mouse.x - lastMousePosition.x;
                const dy = mouse.y - lastMousePosition.y;
                currentCrossline.moveLine(dx, dy);
                lastMousePosition = { x: mouse.x, y: mouse.y };
            } else {
                isDraggingLine = false;
                lastMousePosition = null;
                sendToCgi(crosslines, 'PUT');
            }
        } else if (isDragging && activePoint) {
            if (mouse.button) {
                handlePointDrag(activePoint[0], activePoint[1], mouse);
            } else {
                isDragging = false;
                sendToCgi(crosslines, 'PUT');
            }
        }

        currentCrossline.draw();
        return activePoint;
    }

    /**
     * Handles point dragging with special behavior for first and last points
     * @param {Object} point - Point being dragged
     * @param {number} index - Index of point in crossline
     * @param {Object} mouse - Mouse state data
     */
    function handlePointDrag(point, index, mouse) {
        const dx = mouse.x - mouse.lx;
        const dy = mouse.y - mouse.ly;
        const currentCrossline = crosslines[0];
        const points = currentCrossline.points;

        // Store the initial relative positions if not already stored
        if (!currentCrossline.relativePositions) {
            currentCrossline.relativePositions = calculateRelativePositions(points);
        }

        // If dragging the first or last point
        if (index === 0 || index === points.length - 1) {
            // Move the point being dragged
            point.x += dx;
            point.y += dy;

            // Only adjust intermediate points if we have more than 2 points
            if (points.length > 2) {
                const firstX = points[0].x;
                const firstY = points[0].y;
                const lastX = points[points.length - 1].x;
                const lastY = points[points.length - 1].y;
                const totalWidth = lastX - firstX;

                // Reposition intermediate points based on stored relative positions
                for (let i = 1; i < points.length - 1; i++) {
                    const relPos = currentCrossline.relativePositions[i];

                    // Apply the relative position to the new width
                    points[i].x = firstX + (totalWidth * relPos);

                    // Interpolate Y based on X position
                    const ratio = (points[i].x - firstX) / (lastX - firstX);
                    points[i].y = firstY + ratio * (lastY - firstY);
                }
            }
        } else {
            // For intermediate points, restrict movement to stay within line boundaries
            const prevPoint = points[index - 1];
            const nextPoint = points[index + 1];

            // Calculate new position
            let newX = point.x + dx;
            let newY = point.y + dy;

            // Restrict x-coordinate to stay between neighboring points
            newX = Math.max(prevPoint.x, Math.min(newX, nextPoint.x));

            // For Y coordinate, interpolate based on position along X-axis
            const totalXDistance = nextPoint.x - prevPoint.x;
            const relativeX = (newX - prevPoint.x) / totalXDistance;
            newY = prevPoint.y + (relativeX * (nextPoint.y - prevPoint.y));

            // Update the point position
            point.x = newX;
            point.y = newY;

            // Update the relative positions after movement
            currentCrossline.relativePositions = calculateRelativePositions(points);
        }

        /**
         * Calculates the relative positions of all points
         * @param {Array} points - Array of points
         * @returns {Array} Array of relative positions (0-1)
         */
        function calculateRelativePositions(points) {
            const firstX = points[0].x;
            const lastX = points[points.length - 1].x;
            const totalWidth = lastX - firstX;

            // Calculate relative positions for all points
            return points.map(p => (p.x - firstX) / totalWidth);
        }
    }

    /**
     * Sends crossline data to the server and updates UI
     * @param {Array} crosslines - Array of crossline objects
     * @param {string} method - HTTP method to use
     */
    function sendToCgi(crosslines, method = 'POST') {
        // Create a modified payload that includes direction information
        const currentCrossline = crosslines[0];
        const payload = {
            points: currentCrossline.points,
            direction: currentCrossline.direction
        };

        // console.log(`${name} crossline to cgi: `, payload);

        $.ajax({
            url: path,
            type: 'GET'
        })
            .done(function (response) {
                // console.log(`${name} crossline data:`, response);

                let effectiveMethod = method;
                if (method === 'POST' && response?.data?.length) {
                    effectiveMethod = 'PUT';
                }

                $.ajax({
                    url: path,
                    type: effectiveMethod,
                    data: JSON.stringify(payload),
                    contentType: 'application/json',
                    success: function (response, status) {
                        if (status === 'success') {
                            console.log('Success:', response);
                            if (response.data) {
                                console.log('Processed data:', response.data);
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
                console.error(`Failed to load ${name} crossline data:`, {
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    error: errorThrown
                });
            });
            
        updateUI(currentCrossline.points.length > 0);
    }

    /**
     * Updates UI elements based on crossline state
     * @param {boolean} hasPoints - Whether the crossline has points
     */
    function updateUI(hasPoints) {
        const prefix = name === 'first' ? '1' : '2';
        if (hasPoints) {
            // Set up direction toggle button (remove previous handlers first to prevent duplicates)
            $(`#alternate-crossline${prefix}-button`).off('click').on('click', function () {
                const currentCrossline = crosslines[0];

                // Toggle the direction
                currentCrossline.direction = !currentCrossline.direction;

                // Update the button value
                $(this).val(currentCrossline.direction ? 'forward' : 'reverse');
                // console.log(`Direction toggled to: ${currentCrossline.direction ? 'forward' : 'reverse'}`);

                // Redraw and send updated data
                currentCrossline.draw();
                sendToCgi(crosslines, 'PUT');
            });

            const isFirst = name === 'first';
            const lanePrefix = isFirst ? 'lane1' : 'lane2';
            const laneSlider = document.getElementById(`${lanePrefix}`);
            const laneTextInput = document.querySelector(`input#${lanePrefix}[type="text"]`);

            if (laneSlider && laneTextInput) {
                const newValue = crosslines[0].points.length - 1;
                laneSlider.value = newValue;
                laneTextInput.value = newValue;

                const progress = laneSlider.min == 0 ?
                    (newValue / laneSlider.max) * 100 :
                    ((100 / (laneSlider.max - laneSlider.min)) * newValue) -
                    (100 / (laneSlider.max - laneSlider.min));
                laneSlider.style.background = `linear-gradient(to right, #00fefc ${progress}%, #525252 ${progress}%)`;
            }

            const firstLaneRange = document.querySelector('input#lane1[type="range"]');
            const secondLaneRange = document.querySelector('input#lane2[type="range"]');

            if (firstLaneRange.value === firstLaneRange.min && secondLaneRange.value === secondLaneRange.min) {
                $("#truck-right, #limit-speed").css("display", "none");
            }

            if (name === 'first') {
                $("#control-crossline1-button").removeClass('fa-plus fa-xl fa-solid').addClass('fa-trash fa-lg fa-solid');
                $("#reset-crossline1-button").addClass('fa-rotate-left fa-lg fa-solid');
                $("#alternate-crossline1-button").addClass('fa-right-left fa-rotate-90 fa-lg fa-solid');
                $("#crossline1-caption").text("Crossline 1");
                $("#crossline1").css({
                    "border-style": "solid",
                    "border-color": "#00fefc"
                });
                $("#crossline2, #crossline1-status-dot, #area1-wrongway, #area1-overspeed").css("display", "flex");
                $("#lane1-panel, #wrong-way, #over-speed").css("display", "block");

                if (laneSlider.value > 1) {
                    $("#truck-right, #limit-speed").css("display", "block");
                    $("#area1-truckright, #area1-limitspeed").css("display", "flex");
                } else {
                    $("#area1-truckright, #area1-limitspeed").css("display", "none");
                }
            } else {
                $("#control-crossline2-button").removeClass('fa-plus fa-xl fa-solid').addClass('fa-trash fa-lg fa-solid');
                $("#reset-crossline2-button").addClass('fa-rotate-left fa-lg fa-solid');
                $("#alternate-crossline2-button").addClass('fa-right-left fa-rotate-90 fa-lg fa-solid');
                $("#crossline2-caption").text("Crossline 2");
                $("#crossline2").css({
                    "border-style": "solid",
                    "border-color": "#00fefc"
                });
                $("#crossline2-status-dot, #area2-wrongway, #area2-overspeed").css("display", "flex");
                $("#lane2-panel").css("display", "block");

                if (laneSlider.value > 1) {
                    $("#area2-truckright, #area2-limitspeed").css("display", "flex");
                } else {
                    $("#area2-truckright, #area2-limitspeed").css("display", "none");
                }
            }
        } else {
            if (name === 'first') {
                $("#control-crossline1-button").removeClass('fa-trash fa-lg fa-solid').addClass('fa-plus fa-xl fa-solid');
                $("#reset-crossline1-button").removeClass("fa-rotate-left fa-lg fa-solid");
                $("#alternate-crossline1-button").removeClass('fa-right-left fa-rotate-90 fa-lg fa-solid');
                $("#crossline1-caption").text("Include crossline 1");
                $("#crossline1").css({
                    "border-style": "dashed",
                    "border-color": "rgb(122, 122, 122)"
                });
                $("#crossline2, #crossline1-status-dot, #lane1-panel, #wrong-way, #area1-wrongway, #truck-right , #area1-truckright, #over-speed, #area1-overspeed, #limit-speed, #area1-limitspeed").css("display", "none");

                window.toggleManager.sendWrongwayToCgi($("#area1-wrongway"), false);
                window.toggleManager.sendTruckrightToCgi($("#area1-truckright"), false);
            } else {
                $("#control-crossline2-button").removeClass('fa-trash fa-lg fa-solid').addClass('fa-plus fa-xl fa-solid');
                $("#reset-crossline2-button").removeClass("fa-rotate-left fa-lg fa-solid");
                $("#alternate-crossline2-button").removeClass('fa-right-left fa-rotate-90 fa-lg fa-solid');
                $("#crossline2-caption").text("Include crossline 2");
                $("#crossline2").css({
                    "border-style": "dashed",
                    "border-color": "rgb(122, 122, 122)"
                });
                $("#crossline2-status-dot, #lane2-panel, #area2-wrongway, #area2-truckright, #area2-overspeed, #area2-limitspeed").css("display", "none");

                window.toggleManager.sendWrongwayToCgi($("#area2-wrongway"), false);
                window.toggleManager.sendTruckrightToCgi($("#area2-truckright"), false);
            }
        }
    }

    /**
     * Sets default points for the crossline based on lane configuration
     * @param {number} startX - Starting X coordinate
     */
    function setDefaultPoints(startX) {
        // Clear existing points
        crosslines = [createCrossline([])];

        const endX = startX + 357;
        const y = 384;
        const isFirst = name === 'first';

        // Get lane configuration elements
        const lanePrefix = isFirst ? 'lane1' : 'lane2';
        const laneSlider = document.getElementById(`${lanePrefix}`);
        const laneValue = document.getElementById(`${lanePrefix}`);

        // Default configuration
        let points = [];
        let laneCount = 1;

        // Determine lane count if controls exist
        if (laneSlider && laneValue && laneSlider.value === laneValue.value) {
            laneCount = parseInt(laneSlider.value);
        }

        // Generate points based on lane count
        const segmentWidth = 357 / laneCount;

        // Always add first point
        points.push({ x: startX, y, button: false, lx: startX, ly: y, update: true });

        // Add intermediate points if more than one lane
        for (let i = 1; i < laneCount; i++) {
            const x = Math.round(startX + segmentWidth * i);
            points.push({ x, y, button: false, lx: x, ly: y, update: true });
        }

        // Always add last point if not already added
        if (points[points.length - 1].x !== endX) {
            points.push({ x: endX, y, button: false, lx: endX, ly: y, update: true });
        }

        // Add all points to the crossline
        points.forEach(point => crosslines[0].addPoint(point));

        // Update the UI
        updateUI(true);
    }

    // Manager interface
    const manager = {
        reset() {
            crosslines = [createCrossline([])];
            isDragging = false;
            isDraggingLine = false;
            activePoint = null;
            lastMousePosition = null;
        },
        setDefault() {
            isDefault = true;
        },
        getDefault() {
            return isDefault;
        },
        clearDefault() {
            isDefault = false;
        },
        getCrosslines() {
            return crosslines;
        },
        update,
        setDefaultPoints,
        sendToCgi,
        toggleDirection() {
            crosslines[0].toggleDirection();
            sendToCgi(crosslines, 'PUT');
        }
    };

    // Initialize with data from the server
    $.ajax({
        url: path,
        type: 'GET'
    })
        .done(function (response) {
            if (response?.data) {
                manager.reset();

                // Check if the response has direction information
                if (response.data.points && Array.isArray(response.data.points)) {
                    // New format with points and direction
                    response.data.points.forEach(point => crosslines[0].addPoint(point));
                    if (response.data.direction !== undefined) {
                        crosslines[0].direction = response.data.direction;
                    }
                } else if (Array.isArray(response.data)) {
                    // Old format with just points array
                    response.data.forEach(point => crosslines[0].addPoint(point));
                }

                manager.sendToCgi(crosslines, 'PUT');
            }
        })
        .fail(function (jqXHR, textStatus, errorThrown) {
            console.error(`Failed to load ${name} crossline data:`, {
                status: jqXHR.status,
                statusText: jqXHR.statusText,
                responseText: jqXHR.responseText,
                error: errorThrown
            });
        });

    return manager;
};