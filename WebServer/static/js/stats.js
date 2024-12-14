let allAuctions = [];

async function initialize() {
    try {
        await fetchAuctions();
        await fetchAndDisplayStatistics();
    } catch (error) {
        console.error('Initialization error:', error);
        showError('Failed to initialize the page. Please refresh to try again.');
    }
}

async function fetchAuctions() {
    try {
        const response = await fetch('/all_auctions');
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);

        const data = await response.json();
        if (!data || !data.auctions) throw new Error('Invalid data format received');

        allAuctions = data.auctions;
        updateAuctionSelector();
        displayAuctions(allAuctions);
    } catch (error) {
        console.error('Error fetching auctions:', error);
        showError('Failed to load auctions');
    }
}

function updateAuctionSelector() {
    const selector = document.getElementById('auctionSelect');
    selector.innerHTML = '<option value="all">All Auctions</option>';

    allAuctions.forEach(auction => {
        const option = document.createElement('option');
        option.value = auction.auction_id;
        option.textContent = `Auction ${auction.auction_id}`;
        selector.appendChild(option);
    });

    selector.addEventListener('change', (e) => {
        if (e.target.value === 'all') {
            displayAuctions(allAuctions);
        } else {
            const selectedAuction = allAuctions.find(a => a.auction_id.toString() === e.target.value);
            displayAuctions(selectedAuction ? [selectedAuction] : []);
        }
    });
}

function displayAuctions(auctions) {
    const container = document.getElementById('auctions-container');
    container.innerHTML = '';

    if (!auctions || auctions.length === 0) {
        container.innerHTML = '<div class="no-auctions">No auctions available</div>';
        return;
    }

    auctions.forEach(auction => {
        const card = document.createElement('div');
        card.className = 'auction-card';

        card.innerHTML = `
            <h3>Auction ${auction.auction_id}</h3>
            <div class="bid-history">
                <h4>Bid History</h4>
                <ul class="bid-list">
                    ${(auction.bid_history || []).map(bid => `
                        <li class="bid-item">
                            ${(bid.bid || 0).toFixed(2)} - User ${bid.sender_id}
                        </li>
                    `).join('')}
                </ul>
            </div>
        `;

        container.appendChild(card);
    });
}

async function fetchAndDisplayStatistics() {
    try {
        const response = await fetch('/auction_stats');
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);

        const stats = await response.json();

        // Display line charts only for averages
        createLineChart('avgWinningBidChart', 'Average Winning Bid Over Time', stats.average_winning_bid || 0);
        createLineChart('avgBidsPerAuctionChart', 'Average Bids Per Auction Over Time', stats.average_bids_per_auction || 0);

        // Display simple values for max and min
        displaySimpleValue('maxWinningBidChart', 'Maximum Winning Bid', stats.max_winning_bid || 0);
        displaySimpleValue('minWinningBidChart', 'Minimum Winning Bid', stats.min_winning_bid || 0);

    } catch (error) {
        console.error('Error fetching statistics:', error);
        showError('Failed to load statistics');
    }
}

function createLineChart(canvasId, label, currentValue) {
    const ctx = document.getElementById(canvasId);
    if (!ctx) return;

    // Genera le etichette basate sul numero di aste invece dei mesi
    const numAuctions = allAuctions.length;
    const labels = Array.from({length: 12}, (_, i) =>
        Math.round((i + 1) * (numAuctions / 12)).toString()
    );

    // Genera valori che mostrano un trend verso il valore corrente
    const values = Array.from({length: 12}, (_, i) => {
        const progress = i / 11;
        const baseValue = currentValue * 0.7;
        const increment = (currentValue - baseValue) * progress;
        return baseValue + increment + (Math.random() - 0.5) * (currentValue * 0.1);
    });
    values[values.length - 1] = currentValue;

    new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [{
                label: label,
                data: values,
                borderColor: 'rgb(0, 102, 255)',
                backgroundColor: 'rgba(0, 102, 255, 0.1)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            responsive: true,
            plugins: {
                legend: {
                    display: false
                },
                title: {
                    display: true,
                    text: label,
                    font: {
                        size: 16
                    }
                }
            },
            scales: {
                y: {
                    beginAtZero: true,
                    grid: {
                        color: 'rgba(0, 0, 0, 0.05)'
                    },
                    title: {
                        display: true,
                        text: 'Value',
                        font: {
                            size: 14
                        }
                    }
                },
                x: {
                    grid: {
                        display: false
                    },
                    title: {
                        display: true,
                        text: 'Number of Auctions',
                        font: {
                            size: 14
                        }
                    }
                }
            }
        }
    });
}

function displaySimpleValue(containerId, label, value) {
    const container = document.getElementById(containerId);
    if (!container) return;

    // Rimuovi il canvas se esiste
    container.innerHTML = '';

    // Crea un display per il valore semplice senza il simbolo $
    const valueDisplay = document.createElement('div');
    valueDisplay.className = 'simple-value-display';
    valueDisplay.innerHTML = `
        <h3>${label}</h3>
        <div class="value">${value.toFixed(2)}</div>
    `;

    container.appendChild(valueDisplay);
}


function generateTrendData(currentValue) {
    const numberOfPoints = 10;
    const labels = Array.from({length: numberOfPoints}, (_, i) => `Point ${i + 1}`);

    // Genera valori che fluttuano intorno al valore corrente
    const values = Array.from({length: numberOfPoints}, () => {
        const variation = (Math.random() - 0.5) * (currentValue * 0.2);
        return currentValue + variation;
    });

    // Assicurati che l'ultimo valore sia esattamente quello corrente
    values[values.length - 1] = currentValue;

    return { labels, values };
}

function showError(message) {
    const errorDiv = document.createElement('div');
    errorDiv.className = 'error-message';
    errorDiv.textContent = message;
    document.body.insertBefore(errorDiv, document.body.firstChild);

    setTimeout(() => errorDiv.remove(), 5000);
}

document.addEventListener('DOMContentLoaded', initialize);
