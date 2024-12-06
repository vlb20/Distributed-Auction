// Fetch all auctions and display them
async function fetchAuctions() {
    try {
        const response = await fetch('/all_auctions');
        const data = await response.json();
        const container = document.getElementById('auctions-container');

        data.auctions.forEach(auction => {
            const card = document.createElement('div');
            card.classList.add('auction-card');
            card.innerHTML = `
                <h3>Auction ID: ${auction.auction_id}</h3>
                <p><strong>Item:</strong> ${auction.item ? auction.item.name : "Unknown"}</p>
                <p><strong>Highest Bid:</strong> ${auction.highest_bid}</p>
                <p><strong>Winner ID:</strong> ${auction.winner_id}</p>
                <p><strong>Status:</strong> ${auction.is_active ? "Active" : "Concluded"}</p>
                <p><strong>Bids:</strong></p>
                <ul>
                    ${auction.bid_history.map(bid => `<li>Bid: ${bid.bid}, Sender: ${bid.sender_id}</li>`).join('')}
                </ul>
            `;
            container.appendChild(card);
        });
    } catch (error) {
        console.error('Error fetching auctions:', error);
    }
}

// Fetch auction statistics and display the chart
async function fetchStatistics() {
    try {
        const response = await fetch('/auction_stats');
        const stats = await response.json();

        const ctx = document.getElementById('statsChart').getContext('2d');
        new Chart(ctx, {
            type: 'bar',
            data: {
                labels: ['Avg Winning Bid', 'Avg Bids/Auction', 'Max Winning Bid', 'Min Winning Bid', 'Active Auctions', 'Concluded Auctions'],
                datasets: [{
                    label: 'Auction Statistics',
                    data: [
                        stats.average_winning_bid,
                        stats.average_bids_per_auction,
                        stats.max_winning_bid,
                        stats.min_winning_bid,
                        stats.total_active_auctions,
                        stats.total_concluded_auctions
                    ],
                    backgroundColor: [
                        'rgba(75, 192, 192, 0.2)',
                        'rgba(255, 99, 132, 0.2)',
                        'rgba(54, 162, 235, 0.2)',
                        'rgba(255, 206, 86, 0.2)',
                        'rgba(153, 102, 255, 0.2)',
                        'rgba(255, 159, 64, 0.2)'
                    ],
                    borderColor: [
                        'rgba(75, 192, 192, 1)',
                        'rgba(255, 99, 132, 1)',
                        'rgba(54, 162, 235, 1)',
                        'rgba(255, 206, 86, 1)',
                        'rgba(153, 102, 255, 1)',
                        'rgba(255, 159, 64, 1)'
                    ],
                    borderWidth: 1
                }]
            },
            options: {
                scales: {
                    y: {
                        beginAtZero: true
                    }
                }
            }
        });

    } catch (error) {
        console.error('Error fetching statistics:', error);
    }
}

// Initialize the page
fetchAuctions();
fetchStatistics();