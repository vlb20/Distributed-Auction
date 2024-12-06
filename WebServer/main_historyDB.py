from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from pymongo import MongoClient
from fastapi.responses import HTMLResponse
from fastapi import Request
from fastapi.templating import Jinja2Templates
from fastapi.staticfiles import StaticFiles
import uvicorn

app = FastAPI()

# Connessione a MongoDB
client = MongoClient("mongodb://localhost:27017/")
db = client["auction_db"]
auction_collection = db["auctions"]

# Stato dell'asta in memoria
auction_state = {
    "is_active": False,
    "highest_bid": 0,
    "winner_id": -1,
    "sequence_number": 0,
    "bid_history": []
}

class AuctionMessage(BaseModel):
    bid: int
    highest_bid: int
    message_id: int
    sender_id: int
    winner_id: int
    sequence_number: int
    message_type: str

class ESPCommand(BaseModel):
    command: str
    value: int

app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")

@app.post("/send-command")
async def send_command(command: ESPCommand):
    print(f"Received command request: {command}")
    response = {"status": "command sent", "command": command}
    print(f"Sending response: {response}")
    return response

@app.post("/receive-data")
async def receive_data(message: AuctionMessage):
    print(f"Received auction message: {message}")
    try:
        if message.message_type == "order":
            print(f"Processing bid from sender {message.sender_id} with amount {message.bid}")
            auction_state["highest_bid"] = message.highest_bid
            auction_state["bid_history"].append({"bid": message.bid, "sender_id": message.sender_id, "sequence_number": message.sequence_number})
            auction_state["winner_id"] = message.winner_id

        elif message.message_type == "start":
            print("Starting new auction")
            auction_state["is_active"] = True
            auction_state["highest_bid"] = 0
            auction_state["winner_id"] = -1
            auction_state["sequence_number"] = 0
            auction_state["bid_history"] = []

        elif message.message_type == "end":
            print(f"Ending auction. Winner: {message.winner_id}, Final bid: {message.highest_bid}")
            auction_state["is_active"] = False
            auction_state["winner_id"] = message.winner_id
            auction_state["highest_bid"] = message.highest_bid

            # Salvataggio dello storico nel DB
            auction_collection.insert_one({
                "auction_id": auction_state["sequence_number"],  # Genera ID univoco per l'asta
                "highest_bid": auction_state["highest_bid"],
                "winner_id": auction_state["winner_id"],
                "bid_history": auction_state["bid_history"],
                "is_active": False  # L'asta è terminata
            })

        response = {"status": "message received", "current_state": auction_state}
        print(f"Sending response: {response}")
        return response

    except Exception as e:
        print(f"Error processing message: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/auction_state")
async def get_auction_state():
    print("Received request for auction state")
    print(f"Sending current auction state: {auction_state}")
    return auction_state

@app.get("/bids_history")
async def get_bid_history():
    print("Received request for bid history")
    print(f"Sending bid history: {auction_state['bid_history']}")
    return auction_state["bid_history"]

@app.get("/", response_class=HTMLResponse)
async def get_dashboard(request: Request):
    print("Received request for dashboard page")
    return templates.TemplateResponse("dashboard.html", {"request": request})

if __name__ == "__main__":
    print("Starting server on http://0.0.0.0:8000")
    uvicorn.run(app, host="0.0.0.0", port=8000)
