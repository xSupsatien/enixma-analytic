/**
 * @file chartManager.js
 * @description Handles chart creation, updates, and management
 */

import { config } from "./config.js";

// Chart instances
let totalChart, totalDailyChart, totalWeeklyChart, speedChart, speedDailyChart, speedWeeklyChart;
let initialized = false;

/**
 * Creates and manages chart system
 * @return {Object} Chart manager with public methods
 */
export const createChartManager = () => {
    // Initialize chart system
    const initialize = () => {
        // Initialize chart instances
        initializeCharts();

        // Initial load of data
        getDataChart();

        // Add window resize listener to make charts responsive
        window.addEventListener('resize', handleResize);

        // Refresh data every 1 seconds
        // const refreshInterval = setInterval(getDataChart, 1000);

        initialized = true;

        return { refreshInterval: null };
    };

    // Handle window resize
    const handleResize = () => {
        if (totalChart) totalChart.resize();
        if (totalDailyChart) totalDailyChart.resize();
        if (totalWeeklyChart) totalWeeklyChart.resize();
        if (speedChart) speedChart.resize();
        if (speedDailyChart) speedDailyChart.resize();
        if (speedWeeklyChart) speedWeeklyChart.resize();
    };

    // Clean up charts before recreation
    const cleanupCharts = () => {
        // Dispose of existing chart instances
        if (totalChart) totalChart.dispose();
        if (totalDailyChart) totalDailyChart.dispose();
        if (totalWeeklyChart) totalWeeklyChart.dispose();
        if (speedChart) speedChart.dispose();
        if (speedDailyChart) speedDailyChart.dispose();
        if (speedWeeklyChart) speedWeeklyChart.dispose();

        // Remove any added animation elements
        const chartContainers = [
            'total-chart', 'total-daily-chart', 'total-weekly-chart',
            'speed-chart', 'speed-daily-chart', 'speed-weekly-chart'
        ];

        chartContainers.forEach(containerId => {
            const container = document.getElementById(containerId);
            if (container) {
                // Clear any added animation elements
                const animationElements = container.querySelectorAll('.chart-dotted-circle, .speed-ring');
                animationElements.forEach(element => element.remove());
            }
        });

        // Remove animation styles
        ['chart-animation-style', 'daily-chart-animation-style',
            'speed-chart-animation-style', 'speed-daily-chart-animation-style'].forEach(styleId => {
                const styleElem = document.getElementById(styleId);
                if (styleElem) {
                    document.head.removeChild(styleElem);
                }
            });
    };

    // Function to initialize or reinitialize chart instances
    const initializeCharts = () => {
        // Initialize ECharts instances
        let totalChartDom = document.getElementById('total-chart');
        totalChart = echarts.init(totalChartDom);

        let totalDailyChartDom = document.getElementById('total-daily-chart');
        totalDailyChart = echarts.init(totalDailyChartDom);

        let totalWeeklyChartDom = document.getElementById('total-weekly-chart');
        totalWeeklyChart = echarts.init(totalWeeklyChartDom);

        let speedChartDom = document.getElementById('speed-chart');
        speedChart = echarts.init(speedChartDom);

        let speedDailyChartDom = document.getElementById('speed-daily-chart');
        speedDailyChart = echarts.init(speedDailyChartDom);

        let speedWeeklyChartDom = document.getElementById('speed-weekly-chart');
        speedWeeklyChart = echarts.init(speedWeeklyChartDom);
    };

    // Reset and recreate charts
    const resetCharts = () => {
        cleanupCharts();
        initializeCharts();
        getDataChart();
    };

    // Get sample data and update charts
    const getDataChart = () => {
        // Default data objects for error cases
        const defaults = {
            vehicleCounts: { type: ["Bike", "Car", "Pickup", "Taxi", "Bus", "Truck", "Trailer"], quantity: Array(7).fill(0) },
            dailyCounts: { type: "Total", quantity: Array(24).fill(0) },
            weeklyCounts: { type: "Total", quantity: Array(7).fill(0) },
            averageSpeed: { type: "Speed", quantity: Array(1).fill(0) },
            dailySpeed: { type: "Speed", quantity: Array(24).fill(0) },
            weeklySpeed: { type: "Speed", quantity: Array(7).fill(0) },
            incidentImages: { type: "Incidents", quantity: Array() }
        };

        // Helper function to fetch data and handle errors consistently
        const fetchChartData = (url, updateFunction, defaultData) => {
            return $.ajax({
                url: url,
                type: 'GET',
                dataType: 'json',
                timeout: 5000 // Add timeout to prevent long-hanging requests
            })
                .done(response => updateFunction(response.data))
                .fail((jqXHR, textStatus, errorThrown) => {
                    updateFunction(defaultData);
                    console.error(`Failed to load data from ${url}:`, {
                        status: jqXHR.status,
                        statusText: jqXHR.statusText,
                        responseText: jqXHR.responseText,
                        error: errorThrown
                    });
                });
        };

        // Create an array of promises for all data fetching operations
        const requests = [
            fetchChartData(
                $("#pcu-toggly, #display-pcu-toggly").prop("checked") ? config.paths.vehiclePCU : config.paths.vehicleCounts,
                update_totalChart,
                defaults.vehicleCounts
            ),
            fetchChartData(
                $("#pcu-toggly, #display-pcu-toggly").prop("checked") ? config.paths.dailyPCU : config.paths.dailyCounts,
                update_totalDailyChart,
                defaults.dailyCounts
            ),
            fetchChartData(
                $("#pcu-toggly, #display-pcu-toggly").prop("checked") ? config.paths.weeklyPCU : config.paths.weeklyCounts,
                update_totalWeeklyChart,
                defaults.weeklyCounts
            ),
            fetchChartData(
                config.paths.averageSpeed,
                update_speedChart,
                defaults.averageSpeed
            ),
            fetchChartData(
                config.paths.dailySpeed,
                update_speedDailyChart,
                defaults.dailySpeed
            ),
            fetchChartData(
                config.paths.weeklySpeed,
                update_speedWeeklyChart,
                defaults.weeklySpeed
            ),
            fetchChartData(
                config.paths.incidentImages,
                update_incidentImages,
                defaults.incidentImages
            )
        ];

        // Optional: Add a completion handler for all requests
        Promise.all(requests.map(promise => {
            // Convert jQuery promises to native promises
            return promise.then(
                data => ({ status: 'fulfilled', value: data }),
                error => ({ status: 'rejected', reason: error })
            );
        }))
            .then(results => {
                const failedRequests = results.filter(result => result.status === 'rejected').length;
                if (failedRequests > 0) {
                    console.warn(`${failedRequests} chart data requests failed.`);
                }
            });
    };

    // Clean up resources when no longer needed
    const destroy = () => {
        window.removeEventListener('resize', handleResize);
        cleanupCharts();
        initialized = false;
    };

    // Return public API
    return {
        initialize,
        cleanupCharts,
        resetCharts,
        destroy,
        isInitialized: () => initialized,
        getCharts: () => ({
            totalChart,
            totalDailyChart,
            totalWeeklyChart,
            speedChart,
            speedDailyChart,
            speedWeeklyChart
        })
    };
};

// Chart update functions - These are kept inside the module scope but outside the returned API
function update_totalChart(jsonData) {
    var data = jsonData;
    var total = `${$("#pcu-toggly, #display-pcu-toggly").prop("checked") ? 'PCU' : 'Total'}\nVehicles\n`;

    // Use the specified color palette
    var colorPalette = [
        '#00fefc', // primary
        '#33fefd', // primary-light
        '#00cbca', // primary-dark
        '#00a5a4',
        '#008b8a',
        '#007271',
        '#005a59'
    ];

    // Create gradients using the specified palette
    var chartData = data.type.map((type, index) => {
        // Create linear gradients with the teal palette
        var gradientColor = new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            {
                offset: 0,
                color: colorPalette[index % colorPalette.length]
            },
            {
                offset: 1,
                color: getGradientEndColor(colorPalette[index % colorPalette.length])
            }
        ]);

        return {
            value: data.quantity[index],
            name: type,
            itemStyle: {
                color: gradientColor,
                borderWidth: 0
            },
            label: {
                color: '#b8b8b8',
                fontSize: 13
            }
        };
    });

    // Helper function to create a darker gradient end color
    function getGradientEndColor(startColor) {
        // Create a slightly darker version of the start color
        var darkColor = startColor;

        // Simple approach to darken: if it's already the darkest color, keep it
        if (startColor === '#005a59') {
            return startColor;
        }

        // Otherwise, shift to the next darker color in the palette
        var currentIndex = colorPalette.indexOf(startColor);
        if (currentIndex !== -1 && currentIndex < colorPalette.length - 1) {
            darkColor = colorPalette[currentIndex + 1];
        }

        return darkColor;
    }

    // Chart configuration
    var option = {
        backgroundColor: 'transparent',
        title: {
            text: `${$("#pcu-toggly, #display-pcu-toggly").prop("checked") ? 'PCU' : 'Total'} & Classification`,
            left: 'center',
            textStyle: {
                fontSize: 20,
                color: '#00fefc',
                fontFamily: "'Courier New', monospace"
            }
        },
        tooltip: {
            trigger: 'item',
            formatter: '{a} <br/>{b}: {c} ({d}%)',
            textStyle: {
                color: '#b8b8b8',
                fontFamily: "'Courier New', monospace"
            },
            backgroundColor: 'rgba(41, 41, 41, 0.8)',
            borderColor: '#00fefc',
            borderWidth: 1
        },
        graphic: [
            // Center text
            {
                type: 'text',
                left: 'center',
                top: 'center',
                style: {
                    text: total + chartData.reduce((acc, cur) => acc + cur.value, 0).toString(),
                    textAlign: 'center',
                    fill: '#00fefc',
                    fontSize: 18,
                    fontFamily: "'Courier New', monospace",
                    textShadow: '0 0 8px #00fefc'
                }
            }
        ],
        series: [
            {
                name: `Vehicle ${$("#pcu-toggly, #display-pcu-toggly").prop("checked") ? 'PCU' : 'Total'}`,
                type: 'pie',
                radius: ['50%', '65%'],
                avoidLabelOverlap: true,
                label: {
                    show: true,
                    position: 'outside',
                    formatter: '{b}: {c}',
                    fontSize: 13,
                    fontFamily: "'Courier New', monospace",
                    color: '#b8b8b8'
                },
                emphasis: {
                    label: {
                        show: true,
                        fontWeight: 'bold',
                        color: '#00fefc'
                    },
                    itemStyle: {
                        shadowBlur: 10,
                        shadowColor: 'rgba(0, 254, 252, 0.7)'
                    }
                },
                labelLine: {
                    length: 15,
                    length2: 20,
                    lineStyle: {
                        color: '#494949'
                    }
                },
                itemStyle: {
                    shadowBlur: 5,
                    shadowColor: 'rgba(0, 254, 252, 0.3)'
                },
                data: chartData
            }
        ],
        animation: true,
        animationDuration: 1000,
        animationEasing: 'linear'
    };

    totalChart.setOption(option);

    // Combined animations with segments and dotted circle
    setTimeout(function () {
        // Remove any existing CSS for animated elements
        var existingStyle = document.getElementById('chart-animation-style');
        if (existingStyle) {
            document.head.removeChild(existingStyle);
        }

        // Add CSS for combined animations
        var styleElem = document.createElement('style');
        styleElem.id = 'chart-animation-style';
        styleElem.textContent = `
            #total-chart {
                position: relative;
            }
            
            @keyframes rotate {
                from { transform: translate(-50%, -50%) rotate(0deg); }
                to { transform: translate(-50%, -50%) rotate(360deg); }
            }
            
            /* Rotating dotted circle */
            .chart-dotted-circle {
                position: absolute;
                top: 50%;
                left: 50%;
                width: 135px;
                height: 135px;
                border-radius: 50%;
                border: 2px dashed #00fefc;
                transform: translate(-50%, -50%);
                opacity: 0.5;
                animation: rotate 10s linear infinite;
                pointer-events: none;
            }
            
            /* Top-right segment */
            #total-chart::before {
                content: '';
                position: absolute;
                top: 50%;
                left: 50%;
                width: 145px;
                height: 145px;
                border-radius: 50%;
                border: 2px solid transparent;
                border-top: 2px solid #00fefc;
                border-right: 2px solid #00fefc;
                transform: translate(-50%, -50%);
                opacity: 0.7;
                animation: rotate 8s linear infinite;
                pointer-events: none;
            }
            
            /* Bottom-left segment */
            #total-chart::after {
                content: '';
                position: absolute;
                top: 50%;
                left: 50%;
                width: 145px;
                height: 145px;
                border-radius: 50%;
                border: 2px solid transparent;
                border-bottom: 2px solid #00fefc;
                border-left: 2px solid #00fefc;
                transform: translate(-50%, -50%);
                opacity: 0.4;
                animation: rotate 12s linear infinite reverse;
                pointer-events: none;
            }
        `;
        document.head.appendChild(styleElem);

        // Create the dotted circle as a separate element
        var dottedCircle = document.createElement('div');
        dottedCircle.className = 'chart-dotted-circle';
        document.getElementById('total-chart').appendChild(dottedCircle);
    }, 500);
}

function update_totalDailyChart(jsonData) {
    var data = jsonData;
    var total = `Traffic ${$("#pcu-toggly, #display-pcu-toggly").prop("checked") ? 'PCU' : 'Volume'} Today`;

    var option = {
        backgroundColor: 'transparent',
        title: {
            text: total,
            left: 'center',
            textStyle: {
                fontSize: 20,
                color: '#00fefc',
                fontFamily: "'Courier New', monospace"
            }
        },
        textStyle: {
            color: '#b8b8b8',
            fontFamily: "'Courier New', monospace"
        },
        xAxis: {
            type: 'category',
            data: ['00:00', '01:00', '02:00', '03:00', '04:00', '05:00', '06:00', '07:00', '08:00', '09:00', '10:00', '11:00', '12:00',
                '13:00', '14:00', '15:00', '16:00', '17:00', '18:00', '19:00', '20:00', '21:00', '22:00', '23:00'],
            axisLabel: {
                formatter: function (value) {
                    return value;
                },
                color: '#b8b8b8'
            },
            axisLine: {
                lineStyle: {
                    color: '#494949'
                }
            }
        },
        yAxis: {
            type: 'value',
            axisLabel: {
                color: '#b8b8b8',
                formatter: function (value) {
                    return value >= 1000 ? (value / 1000) + 'k' : value;
                }
            },
            axisLine: {
                lineStyle: {
                    color: '#494949'
                }
            },
            splitLine: {
                lineStyle: {
                    color: 'rgba(73, 73, 73, 0.3)'
                }
            }
        },
        tooltip: {
            trigger: 'axis',
            formatter: function (params) {
                return `${params[0].name}<br/>Total ${$("#pcu-toggly, #display-pcu-toggly").prop("checked") ? 'PCU' : 'count'}: ${params[0].value}`;
            },
            textStyle: {
                color: '#b8b8b8',
                fontFamily: "'Courier New', monospace"
            },
            backgroundColor: 'rgba(41, 41, 41, 0.8)',
            borderColor: '#00fefc',
            borderWidth: 1
        },
        series: [{
            name: data.type,
            type: 'line',
            data: data.quantity,
            itemStyle: {
                color: '#00fefc'
            },
            lineStyle: {
                width: 2,
                shadowBlur: 5,
                shadowColor: 'rgba(0, 254, 252, 0.3)'
            },
            areaStyle: {
                color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                    {
                        offset: 0,
                        color: 'rgba(0, 254, 252, 0.1)'
                    },
                    {
                        offset: 1,
                        color: 'rgba(0, 254, 252, 0.3)'
                    }
                ])
            },
            smooth: true,
            emphasis: {
                itemStyle: {
                    borderWidth: 2,
                    shadowBlur: 10,
                    shadowColor: 'rgba(0, 254, 252, 0.7)'
                }
            },
            symbolSize: 5
        }],
        animation: true,
        animationDuration: 1000,
        animationEasing: 'linear'
    };

    totalDailyChart.setOption(option);

    // Add similar animated effects for the line chart
    setTimeout(function () {
        // Add pulsing effect to the line
        var styleElem = document.getElementById('daily-chart-animation-style') || document.createElement('style');
        styleElem.id = 'daily-chart-animation-style';
        styleElem.textContent = `
            @keyframes pulseLine {
                0% { opacity: 0.7; }
                50% { opacity: 1; }
                100% { opacity: 0.7; }
            }
            
            #total-daily-chart path.echarts-line {
                animation: pulseLine 3s ease-in-out infinite;
            }
        `;

        if (!document.getElementById('daily-chart-animation-style')) {
            document.head.appendChild(styleElem);
        }
    }, 500);
}

function update_totalWeeklyChart(jsonData) {
    var data = jsonData;
    var total = `Traffic ${$("#pcu-toggly, #display-pcu-toggly").prop("checked") ? 'PCU' : 'Volume'} 7 days ago`;

    // Get last 7 days
    var dates = [];
    var currentDate = new Date();
    for (var i = 6; i >= 0; i--) {
        var date = new Date(currentDate);
        date.setDate(currentDate.getDate() - i);
        var formattedDate = date.toLocaleDateString('en-US', { weekday: 'short' });
        dates.push(formattedDate);
    }

    // Create gradient colors for each bar
    var barData = data.quantity.map((value, index) => {
        // Create linear gradients using the teal palette
        var gradientColor = new echarts.graphic.LinearGradient(0, 1, 0, 0, [
            {
                offset: 0,
                color: '#005a59'
            },
            {
                offset: 1,
                color: '#00fefc'
            }
        ]);

        return {
            value: value,
            itemStyle: {
                color: gradientColor,
                borderRadius: [5, 5, 0, 0],
                shadowColor: 'rgba(0, 254, 252, 0.3)',
                shadowBlur: 5
            }
        };
    });

    // Chart configuration
    var option = {
        backgroundColor: 'transparent',
        title: {
            text: total,
            left: 'center',
            textStyle: {
                fontSize: 20,
                color: '#00fefc',
                fontFamily: "'Courier New', monospace"
            }
        },
        tooltip: {
            trigger: 'axis',
            formatter: function (params) {
                return `${params[0].name}<br/>Total ${$("#pcu-toggly, #display-pcu-toggly").prop("checked") ? 'PCU' : 'count'}: ${params[0].value}`;
            },
            axisPointer: {
                type: 'shadow',
                shadowStyle: {
                    color: 'rgba(0, 254, 252, 0.1)'
                }
            },
            textStyle: {
                color: 'rgb(184, 184, 184)',
                fontFamily: "'Courier New', monospace"
            },
            backgroundColor: 'rgba(24, 24, 24, 0.7)',
            borderColor: '#00fefc',
            borderWidth: 1
        },
        xAxis: {
            type: 'category',
            data: dates,
            axisLine: {
                lineStyle: {
                    color: '#494949'
                }
            },
            axisTick: {
                alignWithLabel: true,
                lineStyle: {
                    color: '#494949'
                }
            },
            axisLabel: {
                color: 'rgb(184, 184, 184)',
                fontFamily: "'Courier New', monospace",
                fontSize: 12
            }
        },
        yAxis: {
            type: 'value',
            axisLine: {
                lineStyle: {
                    color: '#494949'
                }
            },
            splitLine: {
                lineStyle: {
                    color: 'rgba(73, 73, 73, 0.3)',
                    type: 'dashed'
                }
            },
            axisLabel: {
                color: 'rgb(184, 184, 184)',
                fontFamily: "'Courier New', monospace",
                fontSize: 12,
                formatter: function (value) {
                    return value >= 1000 ? (value / 1000) + 'k' : value;
                }
            }
        },
        series: [
            {
                name: data.type,
                type: 'bar',
                barWidth: '60%',
                data: barData,
                label: {
                    show: true,
                    position: 'top',
                    color: 'rgb(184, 184, 184)',
                    fontFamily: "'Courier New', monospace",
                    fontSize: 12
                },
                emphasis: {
                    itemStyle: {
                        shadowBlur: 15,
                        shadowColor: 'rgba(0, 254, 252, 0.7)'
                    },
                    label: {
                        color: '#00fefc'
                    }
                }
            }
        ],
        animation: true,
        animationDuration: 1000,
        animationEasing: 'linear'
    };

    totalWeeklyChart.setOption(option);
}

function update_speedChart(jsonData) {
    var data = jsonData;
    var average = 0;

    const quantities = data.quantity.filter(val => !isNaN(val));
    if (quantities.length > 0) {
        const total = quantities.reduce((acc, val) => acc + val, 0);
        average = (total / quantities.length).toFixed(2);
    }

    // Create gradient definitions
    var gradients = [
        new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: '#008b8a' },
            { offset: 1, color: '#005a59' }
        ]),
        new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: '#00cbca' },
            { offset: 1, color: '#008b8a' }
        ]),
        new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: '#33fefd' },
            { offset: 1, color: '#00cbca' }
        ]),
        new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: '#00fefc' },
            { offset: 1, color: '#33fefd' }
        ])
    ];

    var option = {
        backgroundColor: 'transparent',
        title: {
            text: 'Average Speed 1 hour ago',
            left: 'center',
            textStyle: {
                fontSize: 20,
                color: '#00fefc',
                fontFamily: "'Courier New', monospace"
            }
        },
        tooltip: {
            formatter: 'Speed: {c} km/h',
            textStyle: {
                color: '#b8b8b8',
                fontFamily: "'Courier New', monospace"
            },
            backgroundColor: 'rgba(41, 41, 41, 0.8)',
            borderColor: '#00fefc',
            borderWidth: 1
        },
        series: [
            {
                type: 'gauge',
                min: 0,
                max: 200,
                axisLine: {
                    lineStyle: {
                        width: 25,
                        color: [
                            [0.2, gradients[0]], // First range with gradient
                            [0.4, gradients[1]], // Second range with gradient
                            [0.6, gradients[2]], // Third range with gradient
                            [1, gradients[3]]    // Final range with gradient
                        ]
                    }
                },
                pointer: {
                    itemStyle: {
                        color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                            { offset: 0, color: '#00fefc' },
                            { offset: 1, color: '#00cbca' }
                        ]),
                    },
                    width: 5,
                    length: '60%',
                    shadowColor: 'rgba(0, 254, 252, 0.5)',
                    shadowBlur: 10
                },
                axisTick: {
                    distance: -25,
                    length: 8,
                    lineStyle: {
                        color: '#1a2c2c',
                        width: 2
                    }
                },
                splitLine: {
                    distance: -25,
                    length: 25,
                    lineStyle: {
                        color: '#1a2c2c',
                        width: 4
                    }
                },
                axisLabel: {
                    color: '#b8b8b8',
                    distance: 35,
                    fontSize: 13,
                    fontFamily: "'Courier New', monospace"
                },
                detail: {
                    valueAnimation: true,
                    formatter: '{value} km/h',
                    color: '#00fefc',
                    offsetCenter: [0, '80%'],
                    fontSize: 20,
                    fontFamily: "'Courier New', monospace",
                    textShadow: '0 0 8px #00fefc',
                    borderRadius: 20,
                    padding: [8, 15]
                },
                data: [
                    {
                        value: average,
                        title: {
                            offsetCenter: ['0%', '-30%'],
                            fontSize: 15
                        }
                    }
                ],
                animation: true,
                animationDuration: 1000,
                animationEasing: 'linear'
            }
        ]
    };

    speedChart.setOption(option);

    // Add animated effects to the gauge chart
    setTimeout(function () {
        // Remove any existing CSS for animated elements
        var existingStyle = document.getElementById('speed-chart-animation-style');
        if (existingStyle) {
            document.head.removeChild(existingStyle);
        }

        // Add CSS for chart animations
        var styleElem = document.createElement('style');
        styleElem.id = 'speed-chart-animation-style';
        styleElem.textContent = `
            #speed-chart {
                position: relative;
                overflow: hidden;
            }
            
            /* Circular progress ring */
            .speed-ring {
                position: absolute;
                top: 50%;
                left: 50%;
                width: 265px;
                height: 265px;
                border-radius: 50%;
                border: 2px solid transparent;
                border-top: 2px solid #00fefc;
                transform: translate(-50%, -50%);
                opacity: 0.6;
                animation: rotate 5s linear infinite;
                pointer-events: none;
                z-index: 1;
            }
            
            @keyframes rotate {
                from { transform: translate(-50%, -50%) rotate(0deg); }
                to { transform: translate(-50%, -50%) rotate(360deg); }
            }
        `;
        document.head.appendChild(styleElem);

        // Create additional elements for animations
        var chartContainer = document.getElementById('speed-chart');

        // Add rotating progress ring
        var progressRing = document.createElement('div');
        progressRing.className = 'speed-ring';
        chartContainer.appendChild(progressRing);
    }, 500);
}

function update_speedDailyChart(jsonData) {
    var data = jsonData;
    var total = 'Average Speed Today';

    var option = {
        backgroundColor: 'transparent',
        title: {
            text: total,
            left: 'center',
            textStyle: {
                fontSize: 20,
                color: '#00fefc',
                fontFamily: "'Courier New', monospace"
            }
        },
        textStyle: {
            color: '#b8b8b8',
            fontFamily: "'Courier New', monospace"
        },
        xAxis: {
            type: 'category',
            data: ['00:00', '01:00', '02:00', '03:00', '04:00', '05:00', '06:00', '07:00', '08:00', '09:00', '10:00', '11:00', '12:00',
                '13:00', '14:00', '15:00', '16:00', '17:00', '18:00', '19:00', '20:00', '21:00', '22:00', '23:00'],
            axisLabel: {
                formatter: function (value) {
                    return value;
                },
                color: '#b8b8b8'
            },
            axisLine: {
                lineStyle: {
                    color: '#494949'
                }
            }
        },
        yAxis: {
            type: 'value',
            axisLabel: {
                color: '#b8b8b8',
                formatter: function (value) {
                    return value >= 1000 ? (value / 1000) + 'k' : value;
                }
            },
            axisLine: {
                lineStyle: {
                    color: '#494949'
                }
            },
            splitLine: {
                lineStyle: {
                    color: 'rgba(73, 73, 73, 0.3)'
                }
            }
        },
        tooltip: {
            trigger: 'axis',
            formatter: function (params) {
                return `${params[0].name}<br/>Speed: ${params[0].value} km/h`;
            },
            textStyle: {
                color: '#b8b8b8',
                fontFamily: "'Courier New', monospace"
            },
            backgroundColor: 'rgba(41, 41, 41, 0.8)',
            borderColor: '#00fefc',
            borderWidth: 1
        },
        visualMap: {
            show: false,
            pieces: [
                {
                    lte: 40,
                    color: '#007a78' // Darkest cyan/teal
                },
                {
                    gt: 40,
                    lte: 80,
                    color: '#009e9c' // Darker cyan/teal
                },
                {
                    gt: 80,
                    lte: 120,
                    color: '#00c8c6' // Medium cyan/teal
                },
                {
                    gt: 120,
                    color: '#00fefc' // Bright cyan - original color
                }
            ]
        },
        series: [{
            name: data.type,
            type: 'line',
            data: data.quantity.map(value => parseFloat(value.toFixed(2))),
            // No explicit itemStyle as visualMap will control it
            lineStyle: {
                width: 2,
                shadowBlur: 5,
                shadowColor: 'rgba(0, 254, 252, 0.3)'
            },
            areaStyle: {
                color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                    {
                        offset: 0,
                        color: 'rgba(0, 254, 252, 0.1)'
                    },
                    {
                        offset: 1,
                        color: 'rgba(0, 254, 252, 0.3)'
                    }
                ])
            },
            smooth: true,
            emphasis: {
                itemStyle: {
                    borderWidth: 2,
                    shadowBlur: 10,
                    shadowColor: 'rgba(0, 254, 252, 0.7)'
                }
            },
            symbolSize: 5,
            markLine: {
                silent: true,
                symbol: ['none', 'none'],
                label: {
                    show: true,
                    color: '#b8b8b8',
                    fontFamily: "'Courier New', monospace"
                },
                lineStyle: {
                    color: 'rgba(73, 73, 73, 0.7)'
                },
                data: [
                    {
                        yAxis: 40
                    },
                    {
                        yAxis: 80
                    },
                    {
                        yAxis: 120
                    }
                ]
            }
        }],
        animation: true,
        animationDuration: 1000,
        animationEasing: 'linear'
    };

    speedDailyChart.setOption(option);

    // Add similar animated effects for the line chart
    setTimeout(function () {
        // Add pulsing effect to the line
        var styleElem = document.getElementById('speed-daily-chart-animation-style') || document.createElement('style');
        styleElem.id = 'speed-daily-chart-animation-style';
        styleElem.textContent = `
            @keyframes pulseSpeedLine {
                0% { opacity: 0.7; }
                50% { opacity: 1; }
                100% { opacity: 0.7; }
            }
            
            #speed-daily-chart path.echarts-line {
                animation: pulseSpeedLine 3s ease-in-out infinite;
            }
        `;

        if (!document.getElementById('speed-daily-chart-animation-style')) {
            document.head.appendChild(styleElem);
        }
    }, 500);
}

function update_speedWeeklyChart(jsonData) {
    var data = jsonData;
    var total = 'Average Speed 7 days ago';

    // Get last 7 days
    var dates = [];
    var currentDate = new Date();
    for (var i = 6; i >= 0; i--) {
        var date = new Date(currentDate);
        date.setDate(currentDate.getDate() - i);
        var formattedDate = date.toLocaleDateString('en-US', { weekday: 'short' });
        dates.push(formattedDate);
    }

    // Create gradient colors for each bar
    var barData = data.quantity.map((value, index) => {
        // Create linear gradients using the teal palette
        var gradientColor = new echarts.graphic.LinearGradient(0, 1, 0, 0, [
            {
                offset: 0,
                color: '#005a59'
            },
            {
                offset: 1,
                color: '#00fefc'
            }
        ]);

        return {
            value: parseFloat(value.toFixed(2)),
            itemStyle: {
                color: gradientColor,
                borderRadius: [5, 5, 0, 0],
                shadowColor: 'rgba(0, 254, 252, 0.3)',
                shadowBlur: 5
            }
        };
    });

    // Chart configuration
    var option = {
        backgroundColor: 'transparent',
        title: {
            text: total,
            left: 'center',
            textStyle: {
                fontSize: 20,
                color: '#00fefc',
                fontFamily: "'Courier New', monospace"
            }
        },
        tooltip: {
            trigger: 'axis',
            formatter: function (params) {
                return `${params[0].name}<br/>Speed: ${params[0].value} km/h`;
            },
            axisPointer: {
                type: 'shadow',
                shadowStyle: {
                    color: 'rgba(0, 254, 252, 0.1)'
                }
            },
            textStyle: {
                color: 'rgb(184, 184, 184)',
                fontFamily: "'Courier New', monospace"
            },
            backgroundColor: 'rgba(24, 24, 24, 0.7)',
            borderColor: '#00fefc',
            borderWidth: 1
        },
        xAxis: {
            type: 'category',
            data: dates,
            axisLine: {
                lineStyle: {
                    color: '#494949'
                }
            },
            axisTick: {
                alignWithLabel: true,
                lineStyle: {
                    color: '#494949'
                }
            },
            axisLabel: {
                color: 'rgb(184, 184, 184)',
                fontFamily: "'Courier New', monospace",
                fontSize: 12
            }
        },
        yAxis: {
            type: 'value',
            axisLine: {
                lineStyle: {
                    color: '#494949'
                }
            },
            splitLine: {
                lineStyle: {
                    color: 'rgba(73, 73, 73, 0.3)',
                    type: 'dashed'
                }
            },
            axisLabel: {
                color: 'rgb(184, 184, 184)',
                fontFamily: "'Courier New', monospace",
                fontSize: 12,
                formatter: function (value) {
                    return value >= 1000 ? (value / 1000) + 'k' : value;
                }
            }
        },
        series: [
            {
                name: data.type,
                type: 'bar',
                barWidth: '60%',
                data: barData,
                label: {
                    show: true,
                    position: 'top',
                    color: 'rgb(184, 184, 184)',
                    fontFamily: "'Courier New', monospace",
                    fontSize: 12
                },
                emphasis: {
                    itemStyle: {
                        shadowBlur: 15,
                        shadowColor: 'rgba(0, 254, 252, 0.7)'
                    },
                    label: {
                        color: '#00fefc'
                    }
                }
            }
        ],
        animation: true,
        animationDuration: 1000,
        animationEasing: 'linear'
    };

    speedWeeklyChart.setOption(option);
}

function formatTimestamp(date) {
    var day = date.getDate();
    var month = date.getMonth() + 1;
    var year = date.getFullYear();
    var hours = date.getHours();
    var minutes = date.getMinutes();
    var seconds = date.getSeconds();

    day = day < 10 ? '0' + day : day;
    month = month < 10 ? '0' + month : month;
    hours = hours < 10 ? '0' + hours : hours;
    minutes = minutes < 10 ? '0' + minutes : minutes;
    seconds = seconds < 10 ? '0' + seconds : seconds;

    return `${day}/${month}/${year} ${hours}:${minutes}:${seconds}`;
}

function formatTimestampCaption(date) {
    const year = date.getFullYear();
    const month = date.getMonth() + 1;
    const day = date.getDate();
    const hours = date.getHours();
    const minutes = date.getMinutes().toString().padStart(2, '0');
    const seconds = date.getSeconds().toString().padStart(2, '0');

    return `${day} ${date.toLocaleString('default', { month: 'long' })} ${year} at ${hours}:${minutes}:${seconds}.`;
}

function update_incidentImages(jsonData) {
    // Check if jsonData is null/undefined or doesn't have the quantity property
    if (!jsonData || !jsonData.quantity) {
        return; // Exit the function early to prevent errors
    }

    const images = jsonData.quantity;

    const incidentsName = [
        "Unknown",
        "Car Accident",
        "Car Breakdown",
        "Car Stop",
        "Road Construction",
        "Road Block",
        "Wrong Way",
        "Truck's In Right Lane.",
        "Speed Exceeding 120 km/hr",
        "Speed ​Doesn't Match The Limit"
    ];

    const sidebarContent = document.getElementById('sidebarContent');

    // Check if sidebarContent exists
    if (!sidebarContent) {
        return;
    }

    sidebarContent.innerHTML = '';

    // Remove any existing modal to avoid duplicates
    const existingModal = document.getElementById('imageModal');
    if (existingModal) {
        existingModal.remove();
    }

    // Create a new modal with inline styles for animations
    const modal = document.createElement('div');
    modal.id = 'imageModal';
    modal.className = 'image-modal';
    modal.style.opacity = '0';
    modal.style.transition = 'opacity 0.5s ease';

    // Create image element with animation styles
    const modalImg = document.createElement('img');
    modalImg.id = 'modalImage';
    modalImg.style.transform = 'scale(0.5)';
    modalImg.style.transition = 'transform 0.6s cubic-bezier(0.175, 0.885, 0.32, 1.275)';

    // Create message element with animation styles
    const message = document.createElement('div');
    message.id = 'modalMessage';
    message.style.transform = 'translateY(50px)';
    message.style.opacity = '0';
    message.style.transition = 'transform 0.5s ease, opacity 0.5s ease';

    // Create close button with animation styles
    const closeBtn = document.createElement('span');
    closeBtn.className = 'close-modal';
    closeBtn.innerHTML = '&times;';
    closeBtn.style.transform = 'scale(0) rotate(-90deg)';
    closeBtn.style.opacity = '0';
    closeBtn.style.transition = 'transform 0.5s ease, opacity 0.5s ease';

    // Append elements to modal
    modal.appendChild(modalImg);
    modal.appendChild(message);
    modal.appendChild(closeBtn);
    document.body.appendChild(modal);

    // Function to close modal with animation
    function closeModalWithAnimation() {
        // Apply closing animations
        modal.style.opacity = '0';
        modalImg.style.transform = 'scale(0.5)';
        message.style.transform = 'translateY(50px)';
        message.style.opacity = '0';
        closeBtn.style.transform = 'scale(0) rotate(-90deg)';
        closeBtn.style.opacity = '0';

        // Remove glow effect
        modalImg.classList.remove('glow-effect');

        // Hide modal after animations complete
        setTimeout(() => {
            modal.style.display = 'none';
        }, 500);
    }

    // Add event listeners for closing
    closeBtn.addEventListener('click', closeModalWithAnimation);
    modal.addEventListener('click', function (event) {
        if (event.target === modal) {
            closeModalWithAnimation();
        }
    });

    // Dynamically load images - first make sure images is an array
    if (!Array.isArray(images)) {
        return;
    }

    images.slice().reverse().forEach(image => {
        // Make sure image is a valid string
        if (!image) {
            return; // Skip this image
        }

        const filename = image.split("-");

        // Validate we have the expected parts
        if (filename.length < 2) {
            return; // Skip this image
        }

        var timestamp = filename[0];
        var incidentType = parseInt(filename[1]);

        // Validate incidentType
        if (isNaN(incidentType) || incidentType < 0 || incidentType >= incidentsName.length) {
            incidentType = 0; // Default to "Unknown"
        }

        var date = new Date(timestamp * 1000);

        // Validate the date
        if (isNaN(date.getTime())) {
            date = new Date(); // Use current date as fallback
        }

        const cyberPanel = document.createElement('div');
        cyberPanel.className = 'cyber-panel';

        const headerPanel = document.createElement('div');
        headerPanel.className = 'header-panel';

        const panelName = document.createElement('p');
        panelName.textContent = `EVENT: ${incidentsName[incidentType]}`;

        const contentPanel = document.createElement('div');
        contentPanel.className = 'content-panel';

        const topicPanel = document.createElement('div');
        topicPanel.className = 'topic-panel';

        const header = document.createElement('h4');
        header.textContent = 'Datetime of the incident:';

        const dateTime = document.createElement('p');
        dateTime.textContent = formatTimestamp(date);

        const imagePanel = document.createElement('img');
        imagePanel.className = 'image-panel';
        imagePanel.src = `images/incident/${image}.jpg`;
        imagePanel.style.cursor = 'pointer';

        // Add error handling for image loading
        imagePanel.onerror = function () {
            this.src = 'images/placeholder.png'; // Replace with your placeholder image
        };

        // Enhanced click handler with more dramatic animations
        imagePanel.addEventListener('click', function () {
            // Reset animation states
            modalImg.style.transform = 'scale(0.5)';
            message.style.transform = 'translateY(50px)';
            message.style.opacity = '0';
            closeBtn.style.transform = 'scale(0) rotate(-90deg)';
            closeBtn.style.opacity = '0';
            modal.style.opacity = '0';

            // Update content
            modalImg.src = this.src;
            message.innerHTML = `${incidentsName[incidentType]} detected on ${formatTimestampCaption(date)}`;

            // Display modal first
            modal.style.display = 'flex';

            // Trigger animations with noticeable delays
            setTimeout(() => {
                // Fade in background
                modal.style.opacity = '1';

                // After background starts fading in, animate the image
                setTimeout(() => {
                    modalImg.style.transform = 'scale(1)';

                    // After image animates, show the message
                    setTimeout(() => {
                        message.style.transform = 'translateY(0)';
                        message.style.opacity = '1';

                        // After message animates, show the close button
                        setTimeout(() => {
                            closeBtn.style.transform = 'scale(1) rotate(0)';
                            closeBtn.style.opacity = '1';

                            // After all elements are visible, add glow effect
                            setTimeout(() => {
                                modalImg.classList.add('glow-effect');
                            }, 200);
                        }, 150);
                    }, 150);
                }, 100);
            }, 10);
        });

        headerPanel.appendChild(panelName);
        cyberPanel.appendChild(headerPanel);

        topicPanel.appendChild(header);
        topicPanel.appendChild(dateTime);
        contentPanel.appendChild(topicPanel);
        contentPanel.appendChild(imagePanel);
        cyberPanel.appendChild(contentPanel);

        sidebarContent.appendChild(cyberPanel);
    });
}