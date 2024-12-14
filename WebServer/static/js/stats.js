let allAuctions = [];

async function initialize() {
    try {
        await fetchAuctions();
        await fetchStatistics();
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
        option.textContent = `Auction ${auction.auction_id} - ${auction.item?.name || "Unknown Item"}`;
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

        const status = auction.is_active ? 'Active' : 'Concluded';
        const statusClass = auction.is_active ? 'status-active' : 'status-concluded';

        card.innerHTML = `
            <h3>Auction ${auction.auction_id}</h3>

            <div class="info-row">
                <span class="info-label">Item:</span>
                <span class="info-value">${auction.item?.name || "Unknown"}</span>
            </div>

            <div class="info-row">
                <span class="info-label">Highest Bid:</span>
                <span class="info-value">$${(auction.highest_bid || 0).toFixed(2)}</span>
            </div>

            <div class="info-row">
                <span class="info-label">Winner:</span>
                <span class="info-value">${auction.winner_id > 0 ? `User ${auction.winner_id}` : 'No winner yet'}</span>
            </div>

            <div class="info-row">
                <span class="info-label">Status:</span>
                <span class="status-badge ${statusClass}">${status}</span>
            </div>

            <div class="bid-history">
                <h4>Bid History</h4>
                <ul class="bid-list">
                    ${(auction.bid_history || []).map(bid => `
                        <li class="bid-item">
                            $${(bid.bid || 0).toFixed(2)} - User ${bid.sender_id}
                        </li>
                    `).join('')}
                </ul>
            </div>
        `;

        container.appendChild(card);
    });
}

async function fetchStatistics() {
    try {
        const response = await fetch('/auction_stats');
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);

        const stats = await response.json();

        const ctx = document.getElementById('statsChart');
        new Chart(ctx, {
            type: 'bar',
            data: {
                labels: ['Avg Winning Bid', 'Avg Bids/Auction', 'Max Winning Bid', 'Min Winning Bid', 'Active', 'Concluded'],
                datasets: [{
                    label: 'Auction Statistics',
                    data: [
                        stats.average_winning_bid || 0,
                        stats.average_bids_per_auction || 0,
                        stats.max_winning_bid || 0,
                        stats.min_winning_bid || 0,
                        stats.total_active_auctions || 0,
                        stats.total_concluded_auctions || 0
                    ],
                    backgroundColor: [
                        'rgba(0, 102, 255, 0.2)',
                        'rgba(76, 175, 80, 0.2)',
                        'rgba(33, 150, 243, 0.2)',
                        'rgba(255, 193, 7, 0.2)',
                        'rgba(0, 200, 83, 0.2)',
                        'rgba(255, 61, 0, 0.2)'
                    ],
                    borderColor: [
                        'rgba(0, 102, 255, 1)',
                        'rgba(76, 175, 80, 1)',
                        'rgba(33, 150, 243, 1)',
                        'rgba(255, 193, 7, 1)',
                        'rgba(0, 200, 83, 1)',
                        'rgba(255, 61, 0, 1)'
                    ],
                    borderWidth: 2
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: true,
                        grid: {
                            color: 'rgba(0, 0, 0, 0.05)'
                        }
                    },
                    x: {
                        grid: {
                            display: false
                        }
                    }
                },
                plugins: {
                    legend: {
                        display: false
                    }
                }
            }
        });
    } catch (error) {
        console.error('Error fetching statistics:', error);
        showError('Failed to load statistics');
    }
}

function showError(message) {
    const errorDiv = document.createElement('div');
    errorDiv.className = 'error-message';
    errorDiv.textContent = message;
    document.body.insertBefore(errorDiv, document.body.firstChild);

    setTimeout(() => errorDiv.remove(), 5000);
}

document.addEventListener('DOMContentLoaded', initialize);