document.addEventListener("DOMContentLoaded", () => {
    function updateDashboard() {
        // Aggiorna stato asta
        fetch('/auction_state')
            .then(response => response.json())
            .then(data => {
                const statusElement = document.getElementById('auction-status');
                statusElement.textContent = data.is_active ? 'ATTIVA' : 'INATTIVA';
                statusElement.className = 'status ' + (data.is_active ? 'active' : 'inactive');

                document.getElementById('highest-bid').textContent = data.highest_bid;
                document.getElementById('winner-id').textContent =
                    data.winner_id === -1 ? '-' : data.winner_id;
            });

        // Aggiorna cronologia offerte
        fetch('/bids_history')
            .then(response => response.json())
            .then(data => {
                const historyDiv = document.getElementById('bid-history');
                // Popola la cronologia delle offerte
                historyDiv.innerHTML = data.map(bid => `
                    <div class="bid-entry">
                        <strong>Nodo ${bid.sender_id}</strong> ha offerto ${bid.bid}€
                        <br>
                        <small>${new Date(bid.timestamp).toLocaleString()}</small>
                    </div>
                `).join('');

                // Scorri automaticamente verso il basso
                historyDiv.scrollTop = historyDiv.scrollHeight;
            });
    }

    // Aggiorna ogni secondo
    setInterval(updateDashboard, 1000);
    updateDashboard();
});
