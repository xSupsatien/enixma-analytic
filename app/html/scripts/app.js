/**
 * @file app.js
 * @description Main application entry point
 */

import { config } from './modules/config.js';
import { createPolygonManager } from './modules/polygonManager.js';
import { createCrosslineManager } from './modules/crosslineManager.js';
import { createVideoManager } from './modules/videoManager.js';
import { createSliderManager, createDoubleSliderManager } from './modules/sliderManager.js';
import { createToggleManager } from './modules/toggleManager.js';
import { createChartManager } from './modules/chartManager.js';

// Store managers globally
let chartManager;

// Initialize app and handle loading transitions
const initializeApp = () => {
    setTimeout(animateTransition, 1000);
};

const animateTransition = () => {
    const loader = document.getElementById('load');
    const mainApp = document.getElementById('app');

    loader.classList.add('fade-in');

    setTimeout(() => {
        loader.style.display = 'none';
        mainApp.hidden = false;
        requestAnimationFrame(() => {
            mainApp.classList.add('visible');

            // Initialize chart system
            chartManager = createChartManager();
            chartManager.initialize();
        });
    }, 800);
};

// Initialize core components
document.addEventListener('DOMContentLoaded', () => {
    initializeApp();
    initializeTabSystem();
    initializeManagers();
    initializeDoubleSliders();
});

// Tab system initialization with automatic rotation between tabs 1-3 (or just 1-2 if no images in tab 3)
function initializeTabSystem() {
    const tabs = document.querySelectorAll('.tab-item');
    const panes = document.querySelectorAll('.tab-panel');
    const videoOverlay = document.getElementById('video-overlay');
    const displayPCU = document.getElementById('display-pcu');
    let rotationInterval = null;
    let currentRotationTab = 0;
    let hasTab3Content = false;
    let rotationPaused = false;

    // Check if tab 3 has image content
    function checkTab3Content() {
        const sidebarContent = document.getElementById('sidebarContent');
        // If sidebar content exists and has at least one child element (an image panel), tab 3 has content
        return sidebarContent && sidebarContent.children.length > 0;
    }

    // Function to activate a specific tab
    function activateTab(index) {
        // Remove active class from all tabs and panes
        document.querySelectorAll('.tab-item.active').forEach(activeTab => {
            activeTab.classList.remove('active');
        });

        document.querySelectorAll('.tab-panel.active').forEach(activePane => {
            activePane.classList.remove('active');
        });

        // Add active class to specified tab and pane
        tabs[index].classList.add('active');
        panes[index].classList.add('active');

        // Display PCU only for the first tab
        if (index === 0) {
            displayPCU.style.display = 'flex';
        } else {
            displayPCU.style.display = 'none';
        }

        // Show/hide video overlay based on index
        videoOverlay.style.display = index === 3 ? 'block' : 'none';

        // Only destroy and recreate charts when tabs 1 or 2 are clicked
        if (index === 0 || index === 1) {
            if (chartManager && chartManager.isInitialized()) {
                chartManager.resetCharts();
            }
        }
    }

    // Start automatic rotation between tabs based on content availability
    function startRotation() {
        if (rotationInterval) {
            clearInterval(rotationInterval);
        }

        rotationInterval = setInterval(() => {
            // Skip rotation if paused due to modal being open
            if (rotationPaused) {
                return;
            }

            // Update the content check - if images are loaded dynamically, we should check each time
            hasTab3Content = checkTab3Content();

            // If tab 3 has content, rotate between tabs 1-3, otherwise just between 1-2
            if (hasTab3Content) {
                currentRotationTab = (currentRotationTab + 1) % 3; // Cycle between 0, 1, 2
            } else {
                currentRotationTab = (currentRotationTab + 1) % 2; // Cycle only between 0, 1
            }
            activateTab(currentRotationTab);
        }, 10000); // 10 seconds
    }

    // Stop automatic rotation
    function stopRotation() {
        if (rotationInterval) {
            clearInterval(rotationInterval);
            rotationInterval = null;
        }
    }

    // Add click event listeners to all tabs
    tabs.forEach((tab, index) => {
        tab.addEventListener('click', () => {
            // Activate the clicked tab
            activateTab(index);

            // Reset rotation state
            currentRotationTab = index;

            // If tab 4 is clicked, stop rotation; otherwise restart rotation
            if (index === 3) {
                stopRotation();
            } else {
                startRotation();
            }
        });
    });

    // Setup modal interaction listeners
    function setupModalListeners() {
        // Find the image modal
        const modal = document.getElementById('imageModal');
        if (!modal) return;

        // Find close button in modal
        const closeBtn = modal.querySelector('.close-modal');

        // Add click event to modal background to handle closing
        modal.addEventListener('click', (event) => {
            if (event.target === modal) {
                rotationPaused = false; // Resume rotation when clicking outside modal content
            }
        });

        // Add click event to close button
        if (closeBtn) {
            closeBtn.addEventListener('click', () => {
                rotationPaused = false; // Resume rotation when close button is clicked
            });
        }

        // Find all image panels that can open the modal
        document.querySelectorAll('.image-panel').forEach(img => {
            img.addEventListener('click', () => {
                rotationPaused = true; // Pause rotation when image is clicked to open modal
            });
        });
    }

    // Watch for dynamically added modals or images
    const observer = new MutationObserver((mutations) => {
        mutations.forEach((mutation) => {
            if (mutation.addedNodes && mutation.addedNodes.length > 0) {
                // Check if we've added either a modal or image panels
                const hasNewModal = Array.from(mutation.addedNodes).some(node =>
                    node.id === 'imageModal' ||
                    (node.querySelectorAll && node.querySelectorAll('.image-panel').length > 0)
                );

                if (hasNewModal) {
                    setupModalListeners();
                }
            }
        });
    });

    // Start observing the document body for any added modals or images
    observer.observe(document.body, { childList: true, subtree: true });

    // Check if tab 3 has content
    hasTab3Content = checkTab3Content();

    // Setup initial modal listeners if modal already exists
    setupModalListeners();

    // Initialize by activating the first tab and starting rotation
    activateTab(0);
    startRotation();
}

// Initialize all managers and set up event handling
function initializeManagers() {
    // DOM elements setup
    const elements = {
        img: document.getElementById('video-content'),
        canvas: document.getElementById('video-overlay')
    };

    // Make canvas context globally available
    window.ctx = elements.canvas.getContext('2d');

    // Initialize video
    const videoManager = createVideoManager(elements.img, config.video.defaultResolution);
    videoManager.initialize();

    // Initialize managers
    const firstPolygonManager = createPolygonManager('first', config.paths.firstPoly);
    const secondPolygonManager = createPolygonManager('second', config.paths.secondPoly);
    window.firstCrosslineManager = createCrosslineManager('first', config.paths.firstCrossline);
    window.secondCrosslineManager = createCrosslineManager('second', config.paths.secondCrossline);
    const confidenceSlider = createSliderManager('confidence', config.paths.confidence);
    const ppmSlider = createSliderManager('ppm', config.paths.ppm);

    const sliderConfig = {
        firstLaneRange: document.querySelector('input#lane1[type="range"]'),
        firstLaneText: document.querySelector('input#lane1[type="text"]'),
        secondLaneRange: document.querySelector('input#lane2[type="range"]'),
        secondLaneText: document.querySelector('input#lane2[type="text"]'),
        confidenceRange: document.querySelector('input#confidence[type="range"]'),
        confidenceText: document.querySelector('input#confidence[type="text"]'),
        pixelPerMeterRange: document.querySelector('input#pixel-per-meter[type="range"]'),
        pixelPerMeterText: document.querySelector('input#pixel-per-meter[type="text"]'),

        pcuCarRange: document.querySelector('input#car-pcu[type="range"]'),
        pcuCarText: document.querySelector('input#car-pcu[type="text"]'),
        pcuBikeRange: document.querySelector('input#bike-pcu[type="range"]'),
        pcuBikeText: document.querySelector('input#bike-pcu[type="text"]'),
        pcuTruckRange: document.querySelector('input#truck-pcu[type="range"]'),
        pcuTruckText: document.querySelector('input#truck-pcu[type="text"]'),
        pcuBusRange: document.querySelector('input#bus-pcu[type="range"]'),
        pcuBusText: document.querySelector('input#bus-pcu[type="text"]'),
        pcuTaxiRange: document.querySelector('input#taxi-pcu[type="range"]'),
        pcuTaxiText: document.querySelector('input#taxi-pcu[type="text"]'),
        pcuPickupRange: document.querySelector('input#pickup-pcu[type="range"]'),
        pcuPickupText: document.querySelector('input#pickup-pcu[type="text"]'),
        pcuTrailerRange: document.querySelector('input#trailer-pcu[type="range"]'),
        pcuTrailerText: document.querySelector('input#trailer-pcu[type="text"]'),

        firstIncidentsRange: document.querySelector('input#incidents1[type="range"]'),
        firstIncidentsText: document.querySelector('input#incidents1[type="text"]'),
        secondIncidentsRange: document.querySelector('input#incidents2[type="range"]'),
        secondIncidentsText: document.querySelector('input#incidents2[type="text"]'),
        firstOverSpeedRange: document.querySelector('input#overspeed1[type="range"]'),
        firstOverSpeedText: document.querySelector('input#overspeed1[type="text"]'),
        secondOverSpeedRange: document.querySelector('input#overspeed2[type="range"]'),
        secondOverSpeedText: document.querySelector('input#overspeed2[type="text"]')
    };
    confidenceSlider.initialize(sliderConfig);
    ppmSlider.initialize(sliderConfig);

    // Initialize toggle manager
    const toggleManager = createToggleManager('incidents', {
        firstIncidents: config.paths.firstIncidents,
        secondIncidents: config.paths.secondIncidents,
        firstWrongWay: config.paths.firstWrongWay,
        secondWrongWay: config.paths.secondWrongWay,
        firstTruckRight: config.paths.firstTruckRight,
        secondTruckRight: config.paths.secondTruckRight,
        firstOverSpeed: config.paths.firstOverSpeed,
        secondOverSpeed: config.paths.secondOverSpeed,
        firstLimitSpeed: config.paths.firstLimitSpeed,
        secondLimitSpeed: config.paths.secondLimitSpeed,
        pcu: config.paths.pcu
    });
    toggleManager.initialize();

    // Make toggle manager globally available if needed
    window.toggleManager = toggleManager;

    // Mouse state setup
    const mouse = {
        x: 0, y: 0, button: 0,
        lx: 0, ly: 0, update: true
    };

    // Mouse event handler
    function mouseEvents(e) {
        const bounds = elements.canvas.getBoundingClientRect();
        mouse.x = e.pageX - bounds.left - scrollX;
        mouse.y = e.pageY - bounds.top - scrollY;
        mouse.button = e.type === 'mousedown' ? true :
            e.type === 'mouseup' ? false :
                mouse.button;
        mouse.update = true;
    }

    ['mousedown', 'mouseup', 'mousemove'].forEach(
        name => elements.canvas.addEventListener(name, mouseEvents)
    );

    function drawCircle(pos, size = 10) {
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, size, 0, Math.PI * 2);
        ctx.stroke();
    }

    // Animation loop
    function update() {
        ctx.clearRect(0, 0, elements.canvas.width, elements.canvas.height);
        ctx.drawImage(elements.img, 0, 0, 1024, 768);

        if (mouse.update) {
            let cursor = 'default';

            // Check if mouse is over any polygon area or crossline first
            const isOverFirstPolygon = firstPolygonManager.isPointInPolygon({ x: mouse.x, y: mouse.y });
            const isOverSecondPolygon = secondPolygonManager.isPointInPolygon({ x: mouse.x, y: mouse.y });
            const isOverFirstCrossline = firstCrosslineManager.getCrosslines()[0].isPointNearLine(mouse);
            const isOverSecondCrossline = secondCrosslineManager.getCrosslines()[0].isPointNearLine(mouse);

            // Set move cursor for polygon areas
            if ((isOverFirstPolygon || isOverSecondPolygon) && !mouse.button) {
                cursor = 'move';
            }

            // Set pointer cursor for crosslines
            if ((isOverFirstCrossline || isOverSecondCrossline) && !mouse.button) {
                cursor = 'grab';
            }

            // If dragging, override with grabbing cursor
            if (mouse.button && (isOverFirstPolygon || isOverSecondPolygon || isOverFirstCrossline || isOverSecondCrossline)) {
                cursor = 'grabbing';
            }

            // Handle polygon managers
            if (firstPolygonManager.getDefault()) {
                firstPolygonManager.setDefaultPoints(20);
                firstPolygonManager.sendToCgi(firstPolygonManager.getPolygons());
                mouse.button = false;
                firstPolygonManager.clearDefault();
            }

            if (secondPolygonManager.getDefault()) {
                secondPolygonManager.setDefaultPoints(527);
                secondPolygonManager.sendToCgi(secondPolygonManager.getPolygons());
                mouse.button = false;
                secondPolygonManager.clearDefault();
            }

            const firstPolygonActivePoint = firstPolygonManager.update(mouse);
            const secondPolygonActivePoint = secondPolygonManager.update(mouse);

            if (firstPolygonActivePoint) {
                drawCircle(firstPolygonActivePoint[0]);
                cursor = 'pointer';
            }

            if (secondPolygonActivePoint) {
                drawCircle(secondPolygonActivePoint[0]);
                cursor = 'pointer';
            }

            // Handle crossline managers
            if (firstCrosslineManager.getDefault()) {
                firstCrosslineManager.setDefaultPoints(80);
                firstCrosslineManager.sendToCgi(firstCrosslineManager.getCrosslines());
                mouse.button = false;
                firstCrosslineManager.clearDefault();
            }

            if (secondCrosslineManager.getDefault()) {
                secondCrosslineManager.setDefaultPoints(587);
                secondCrosslineManager.sendToCgi(secondCrosslineManager.getCrosslines());
                mouse.button = false;
                secondCrosslineManager.clearDefault();
            }

            const firstCrosslineActivePoint = firstCrosslineManager.update(mouse);
            const secondCrosslineActivePoint = secondCrosslineManager.update(mouse);

            if (firstCrosslineActivePoint) {
                drawCircle(firstCrosslineActivePoint[0]);
                cursor = 'pointer';
            }

            if (secondCrosslineActivePoint) {
                drawCircle(secondCrosslineActivePoint[0]);
                cursor = 'pointer';
            }

            mouse.lx = mouse.x;
            mouse.ly = mouse.y;
            elements.canvas.style.cursor = cursor;
        }

        requestAnimationFrame(update);
    }

    // Button event handlers setup
    function initializeButtonHandlers() {
        // Polygon button handlers
        $('#reset-area1-button').click(() => {
            firstPolygonManager.reset();
            firstPolygonManager.setDefault();
        });

        $('#reset-area2-button').click(() => {
            secondPolygonManager.reset();
            secondPolygonManager.setDefault();
        });

        $('#control-area1-button').click(() => {
            firstPolygonManager.reset();

            if ($('#control-area1-button').attr('class') === 'fa-plus fa-xl fa-solid') {
                firstPolygonManager.setDefault();
            } else {
                firstPolygonManager.sendToCgi(firstPolygonManager.getPolygons(), 'DELETE');

                const poly2Points = secondPolygonManager.getPolygons()[0].points;
                if (poly2Points && poly2Points.length > 0) {
                    secondPolygonManager.reset();
                    secondPolygonManager.sendToCgi(secondPolygonManager.getPolygons(), 'DELETE');
                }
            }
        });

        $('#control-area2-button').click(() => {
            secondPolygonManager.reset();

            if ($('#control-area2-button').attr('class') === 'fa-plus fa-xl fa-solid') {
                secondPolygonManager.setDefault();
            } else {
                secondPolygonManager.sendToCgi(secondPolygonManager.getPolygons(), 'DELETE');
            }
        });

        // Crossline button handlers
        $('#reset-crossline1-button').click(() => {
            firstCrosslineManager.reset();
            firstCrosslineManager.setDefault();
        });

        $('#reset-crossline2-button').click(() => {
            secondCrosslineManager.reset();
            secondCrosslineManager.setDefault();
        });

        $('#control-crossline1-button').click(() => {
            firstCrosslineManager.reset();

            if ($('#control-crossline1-button').attr('class') === 'fa-plus fa-xl fa-solid') {
                firstCrosslineManager.setDefault();
            } else {
                firstCrosslineManager.sendToCgi(firstCrosslineManager.getCrosslines(), 'DELETE');

                const crossline2Points = secondCrosslineManager.getCrosslines()[0].points;
                if (crossline2Points && crossline2Points.length > 0) {
                    secondCrosslineManager.reset();
                    secondCrosslineManager.sendToCgi(secondCrosslineManager.getCrosslines(), 'DELETE');
                }
            }
        });

        $('#control-crossline2-button').click(() => {
            secondCrosslineManager.reset();

            if ($('#control-crossline2-button').attr('class') === 'fa-plus fa-xl fa-solid') {
                secondCrosslineManager.setDefault();
            } else {
                secondCrosslineManager.sendToCgi(secondCrosslineManager.getCrosslines(), 'DELETE');
            }
        });

        // Add separate event listener for PCU toggle
        $('#display-pcu-toggly').click(() => {
            if (chartManager && chartManager.isInitialized()) {
                chartManager.resetCharts();
            }
        });
    }

    // Initialize button handlers and start animation
    initializeButtonHandlers();
    requestAnimationFrame(update);
}

// Add this function to initialize the double sliders
function initializeDoubleSliders() {
    // Create double slider managers for both limit speed inputs
    const limitSpeed1 = createDoubleSliderManager('area1-limitspeed', config.paths.firstLimitSpeed);
    const limitSpeed2 = createDoubleSliderManager('area2-limitspeed', config.paths.secondLimitSpeed);

    // Initialize the sliders
    limitSpeed1.initialize('area1-limitspeed');
    limitSpeed2.initialize('area2-limitspeed');
}