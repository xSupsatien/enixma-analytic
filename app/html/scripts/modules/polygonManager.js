/**
 * @file polygonManager.js
 * @description Creates a polygon manager instance with dragging functionality and shared state management
 */

import { sharedState } from './sharedState.js';

/**
 * Creates a polygon manager instance with dragging functionality
 * @param {string} name - Identifier for the polygon manager instance
 * @param {string} path - API endpoint path for polygon data
 * @returns {Object} Polygon manager interface
 */
const createPolygonManager = (name, path) => {
    let polygons = [createPoly([])];
    let isDefault = false;
    let isDragging = false;
    let activePoint = null;
    let isDraggingPolygon = false;
    let lastMousePosition = null;

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
     * Creates a polygon object with drawing and point management capabilities
     * @param {Array} polygonCf - Initial points configuration
     * @returns {Object} Polygon object
     */
    function createPoly(polygonCf) {
        return {
            points: polygonCf,
            addPoint(p) {
                this.points.push(createPoint(Math.round(p.x), Math.round(p.y)));
            },
            draw() {
                ctx.lineWidth = 3;
                ctx.strokeStyle = '#00fefc';
                ctx.fillStyle = 'rgba(0, 254, 252, 0.1)';
                
                // Draw filled polygon
                ctx.beginPath();
                if (this.points.length > 0) {
                    ctx.moveTo(this.points[0].x, this.points[0].y);
                    for (let i = 1; i < this.points.length; i++) {
                        ctx.lineTo(this.points[i].x, this.points[i].y);
                    }
                }
                ctx.closePath();
                ctx.fill();
                ctx.stroke();

                // Draw points
                this.points.forEach((p, index) => {
                    ctx.beginPath();
                    ctx.moveTo(p.x + 4, p.y);
                    const innerRadius = index % 2 === 0 ? 5 : 3;
                    ctx.arc(p.x, p.y, innerRadius, 0, Math.PI * 2);
                    ctx.arc(p.x, p.y, 10, 0, Math.PI * 2);
                    ctx.stroke();
                });
            },
            closest(pos, dist = 8) {
                var i = 0, index = -1;
                dist *= dist;
                for (const p of this.points) {
                    var x = pos.x - p.x;
                    var y = pos.y - p.y;
                    var d2 = x * x + y * y;
                    if (d2 < dist) {
                        dist = d2;
                        index = i;
                    }
                    i++;
                }
                if (index > -1) { return [this.points[index], index] }
            },
            isPointInside(point) {
                let inside = false;
                const x = point.x;
                const y = point.y;
                
                for (let i = 0, j = this.points.length - 1; i < this.points.length; j = i++) {
                    const xi = this.points[i].x;
                    const yi = this.points[i].y;
                    const xj = this.points[j].x;
                    const yj = this.points[j].y;
                    
                    const intersect = ((yi > y) !== (yj > y))
                        && (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
                    if (intersect) inside = !inside;
                }
                
                return inside;
            },
            movePolygon(dx, dy) {
                this.points.forEach(point => {
                    point.x += dx;
                    point.y += dy;
                });
            }
        };
    }

    /**
     * Updates polygon state based on mouse interaction
     * @param {Object} mouse - Mouse position and state data
     * @returns {Array|null} Active point data if any
     */
    function update(mouse) {
        const currentPolygon = polygons[0];
        
        // Always check mouse state first
        sharedState.checkMouseState(mouse);

        // Handle crossline interaction first
        if (isCrosslinePointNearby(mouse)) {
            if (isDragging || isDraggingPolygon) {
                isDragging = false;
                isDraggingPolygon = false;
                lastMousePosition = null;
                sendToCgi(polygons, 'PUT');
            }
            currentPolygon.draw();
            return null;
        }
        
        // Only check for active manager if the mouse button is pressed
        if (mouse.button && sharedState.activeManager && sharedState.activeManager !== 'polygon') {
            // Clear any active state if we had it
            if (isDragging || isDraggingPolygon) {
                isDragging = false;
                isDraggingPolygon = false;
                lastMousePosition = null;
                sendToCgi(polygons, 'PUT');
            }
            currentPolygon.draw();
            return null;
        }
    
        if (!isDragging && !isDraggingPolygon) {
            activePoint = currentPolygon.closest(mouse);
            
            if (mouse.button) {
                if (activePoint) {
                    sharedState.setActiveManager('polygon');
                } else if (currentPolygon.isPointInside(mouse)) {
                    isDraggingPolygon = true;
                    lastMousePosition = { x: mouse.x, y: mouse.y };
                    sharedState.setActiveManager('polygon');
                }
            }
        }
    
        if (isDraggingPolygon) {
            if (mouse.button) {
                const dx = mouse.x - lastMousePosition.x;
                const dy = mouse.y - lastMousePosition.y;
                currentPolygon.movePolygon(dx, dy);
                lastMousePosition = { x: mouse.x, y: mouse.y };
            } else {
                isDraggingPolygon = false;
                lastMousePosition = null;
                sendToCgi(polygons, 'PUT');
            }
        } else if (activePoint) {
            if (mouse.button) {
                if (isDragging) {
                    handlePointDrag(activePoint[0], activePoint[1], mouse);
                } else {
                    isDragging = true;
                    handleOddIndexPoint(activePoint[0], activePoint[1]);
                }
            } else {
                if (isDragging) {
                    sendToCgi(polygons, 'PUT');
                }
                isDragging = false;
            }
        }
    
        currentPolygon.draw();
        return activePoint;
    }
    
    /**
     * Checks if a crossline point or line segment is nearby
     * @param {Object} mouse - Mouse position
     * @returns {boolean} True if crossline interaction is detected
     */
    function isCrosslinePointNearby(mouse) {
        const managers = [
            window.firstCrosslineManager,
            window.secondCrosslineManager
        ];
    
        for (const manager of managers) {
            if (manager) {
                const crosslines = manager.getCrosslines();
                for (const crossline of crosslines) {
                    // Check for nearby points
                    if (crossline.closest(mouse)) {
                        return true;
                    }
                    // Check if mouse is near the line itself
                    if (crossline.isPointNearLine && crossline.isPointNearLine(mouse)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    /**
     * Sends polygon data to the server and updates UI
     * @param {Array} polygons - Array of polygon objects
     * @param {string} method - HTTP method to use
     */
    function sendToCgi(polygons, method = 'POST') {
        const payload = polygons[0].points;
        // console.log(`${name} poly to cgi: `, payload);

        const originalMethod = method;

        $.ajax({
            url: path,
            type: 'GET'
        })
            .done(function (response) {
                // console.log(`${name} polygon data:`, response);

                let effectiveMethod = originalMethod;
                if (originalMethod === 'POST' && response?.data?.length) {
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

                            if (response.error) {
                                console.error('Server error:', response.error);
                                return;
                            }

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
                console.error(`Failed to load ${name} polygon data:`, {
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    error: errorThrown
                });
            });

        updateUI(payload.length > 0);
    }

    /**
     * Updates UI elements based on polygon state
     * @param {boolean} hasPoints - Whether there are points in the polygon
     */
    function updateUI(hasPoints) {
        if (hasPoints) {
            if (name === 'first') {
                $('#control-area1-button').removeClass('fa-plus fa-xl fa-solid').addClass('fa-trash fa-lg fa-solid');
                $('#reset-area1-button').addClass('fa-rotate-left fa-lg fa-solid')
                $("#area1-caption").text("Area 1");
                $("#area1").css({
                    "border-style": "solid",
                    "border-color": "#00fefc"
                });
                $("#area2, #roi1-status-dot, #area1-incidents").css("display", "flex");
                $("#incidents").css("display", "block");
            } else {
                $('#control-area2-button').removeClass('fa-plus fa-xl fa-solid').addClass('fa-trash fa-lg fa-solid');
                $('#reset-area2-button').addClass('fa-rotate-left fa-lg fa-solid')
                $("#area2-caption").text("Area 2");
                $("#area2").css({
                    "border-style": "solid",
                    "border-color": "#00fefc"
                });
                $("#roi2-status-dot, #area2-incidents").css("display", "flex");
            }
        } else {
            if (name === 'first') {
                $('#control-area1-button').removeClass('fa-trash fa-lg fa-solid').addClass('fa-plus fa-xl fa-solid');
                $("#reset-area1-button").removeClass("fa-rotate-left fa-lg fa-solid");
                $("#area1-caption").text("Include area 1");
                $("#area1").css({
                    "border-style": "dashed",
                    "border-color": "rgb(122, 122, 122)"
                });
                $("#area2, #roi1-status-dot, #incidents, #area1-incidents").css("display", "none");

                window.toggleManager.sendIncidentsToCgi($("#area1-incidents"), false);
            }
            else {
                $('#control-area2-button').removeClass('fa-trash fa-lg fa-solid').addClass('fa-plus fa-xl fa-solid');
                $("#reset-area2-button").removeClass("fa-rotate-left fa-lg fa-solid");
                $("#area2-caption").text("Include area 2");
                $("#area2").css({
                    "border-style": "dashed",
                    "border-color": "rgb(122, 122, 122)"
                });
                $("#roi2-status-dot, #area2-incidents").css("display", "none");

                window.toggleManager.sendIncidentsToCgi($("#area2-incidents"), false);
            }
        }
    }

    /**
     * Sets default points for the polygon
     * @param {number} startX - Starting X coordinate
     */
    function setDefaultPoints(startX) {
        const points = [
            { x: startX, y: 20 },
            { x: startX + 239, y: 20 },
            { x: startX + 477, y: 20 },
            { x: startX + 477, y: 384 },
            { x: startX + 477, y: 748 },
            { x: startX + 239, y: 748 },
            { x: startX, y: 748 },
            { x: startX, y: 384 }
        ];

        for (const point of points) {
            polygons[0].addPoint(point);
        }
    }

    /**
     * Handles point dragging logic
     * @param {Object} point - Point being dragged
     * @param {number} index - Index of the point
     * @param {Object} mouse - Mouse position data
     */
    function handlePointDrag(point, index, mouse) {
        const currentPolygon = polygons[0];
        const lastIndex = currentPolygon.points.length - 1;

        const dx = mouse.x - mouse.lx;
        const dy = mouse.y - mouse.ly;
        point.x += dx;
        point.y += dy;

        if (index === 0 || (index % 2 === 0 && index === lastIndex - 1)) {
            const lastPoint = currentPolygon.points[lastIndex];
            const firstPoint = currentPolygon.points[0];
            const lastEvenPoint = currentPolygon.points[lastIndex - 1];

            lastPoint.x = (firstPoint.x + lastEvenPoint.x) / 2;
            lastPoint.y = (firstPoint.y + lastEvenPoint.y) / 2;
        }

        if (index % 2 === 0) {
            if (index > 0) {
                const prevOdd = currentPolygon.points[index - 1];
                const prevEven = currentPolygon.points[index - 2];
                if (prevEven) {
                    prevOdd.x = (prevEven.x + point.x) / 2;
                    prevOdd.y = (prevEven.y + point.y) / 2;
                }
            }

            if (index + 2 < currentPolygon.points.length) {
                const nextOdd = currentPolygon.points[index + 1];
                const nextEven = currentPolygon.points[index + 2];
                nextOdd.x = (point.x + nextEven.x) / 2;
                nextOdd.y = (point.y + nextEven.y) / 2;
            }
        }
    }

    /**
     * Handles special case for odd-indexed points
     * @param {Object} point - Point being processed
     * @param {number} index - Index of the point
     */
    function handleOddIndexPoint(point, index) {
        if (index % 2 === 1) {
            const currentPolygon = polygons[0];

            if (index === currentPolygon.points.length - 1) {
                const prevEvenPoint = currentPolygon.points[index - 1];
                const firstPoint = currentPolygon.points[0];

                const newPrevPoint = {
                    x: (prevEvenPoint.x + point.x) / 2,
                    y: (prevEvenPoint.y + point.y) / 2
                };

                const newLastPoint = {
                    x: (point.x + firstPoint.x) / 2,
                    y: (point.y + firstPoint.y) / 2
                };

                currentPolygon.points.splice(index, 1);
                currentPolygon.points.splice(index, 0, newPrevPoint, point, newLastPoint);

                activePoint[1] = index + 1;
            } else {
                const nextEvenPoint = currentPolygon.points[index + 1];
                const prevEvenPoint = currentPolygon.points[index - 1];

                const newPrevPoint = {
                    x: (prevEvenPoint.x + point.x) / 2,
                    y: (prevEvenPoint.y + point.y) / 2
                };

                const newNextPoint = {
                    x: (point.x + nextEvenPoint.x) / 2,
                    y: (point.y + nextEvenPoint.y) / 2
                };

                currentPolygon.points.splice(index, 1);
                currentPolygon.points.splice(index, 0, newPrevPoint, point, newNextPoint);

                activePoint[1] = index + 1;
            }
        }
    }

    // Manager interface
    const manager = {
        reset() {
            polygons = [createPoly([])];
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
        getPolygons() {
            return polygons;
        },
        update(mouse) {
            return update(mouse);
        },
        setDefaultPoints,
        sendToCgi,
        isPointInPolygon(point) {
            return polygons[0].isPointInside(point);
        }
    };

    // Initialize with data from the server
    $.ajax({
        url: path,
        type: 'GET'
    })
        .done(function (response) {
            // console.log(`${name} polygon data:`, response);

            if (response && response.data && Array.isArray(response.data)) {
                manager.reset();
                for (const point of response.data) {
                    polygons[0].addPoint(point);
                }
                manager.sendToCgi(polygons, 'PUT');
            }
        })
        .fail(function (jqXHR, textStatus, errorThrown) {
            console.error(`Failed to load ${name} polygon data:`, {
                status: jqXHR.status,
                statusText: jqXHR.statusText,
                responseText: jqXHR.responseText,
                error: errorThrown
            });
        });

    return manager;
};

// Export the factory function
export { createPolygonManager };