from fastapi import FastAPI, HTTPException, Form, Request
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
counter_collection = db["counters"]

# Assicurati che esista un contatore per auction_id
if not counter_collection.find_one({"_id": "auction_id"}):
    counter_collection.insert_one({"_id": "auction_id", "sequence_value": 0})

def get_next_sequence_value(sequence_name):
    """
    Genera un ID incrementale basato sul nome del contatore.
    """
    result = counter_collection.find_one_and_update(
        {"_id": sequence_name},
        {"$inc": {"sequence_value": 1}},
        return_document=True
    )
    return result["sequence_value"]

auction_state ={
    "is_active": False,
    "highest_bid": 0,
    "winner_id": -1,
    "sequence_number": 0,
    "bid_history": [],
    "item":{}
}

class AuctionMessage(BaseModel):
    bid: int
    highest_bid: int
    message_id: int
    sender_id: int
    winner_id: int
    sequence_number: int
    message_type: str

app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")

# Endpoint iniziale per la configurazione dell'asta
@app.post("/set-auction")
async def set_auction(request: Request, item_name: str = Form(...), item_description: str = Form(...)):
    print(f"Setting up auction: {item_name}, {item_description}")

    # Inizializza lo stato dell'asta
    auction_state["is_active"] = False
    auction_state["highest_bid"] = 0
    auction_state["winner_id"] = -1
    auction_state["sequence_number"] = 0
    auction_state["bid_history"] = []
    auction_state["item"] = {
        "name": item_name,
        "description": item_description,
    }

     # Passa nome e descrizione al template
    return templates.TemplateResponse("dashboard.html", {
        "request": request,
        "item_name": item_name,
        "item_description": item_description
    })

@app.post("/receive-data")
async def receive_data(message: AuctionMessage):
    print(f"Received auction message: {message}")
    try:
        if message.message_type == "order":
            # Aggiorno lo stato locale dell'asta
            print(f"Processing bid from sender {message.sender_id} with amount {message.bid}")
            auction_state["highest_bid"] = message.highest_bid
            auction_state["bid_history"].append({"bid": message.bid, "sender_id": message.sender_id, "sequence_number": message.sequence_number})
            auction_state["winner_id"] = message.winner_id

            # Aggiorna l'asta nel database
                # Trova l'asta attiva
            auction = auction_collection.find_one({"is_active": True})
            if not auction:
                raise HTTPException(status_code=404, detail="No active auction found")

                # Aggiorna l'asta con il nuovo bid
            updated_bid = {
                "bid": message.bid,
                "sender_id": message.sender_id,
                "sequence_number": auction["sequence_number"] + 1  # Incrementa il sequence_number
            }
            auction_collection.update_one(
                {"auction_id": auction["auction_id"]},
                {"$set": {
                    "highest_bid": max(message.bid, auction["highest_bid"]),
                    "winner_id": message.sender_id,
                    "sequence_number": updated_bid["sequence_number"]
                },
                "$push": {
                    "bid_history": updated_bid
                }}
            )

        elif message.message_type == "start":
            # Aggiorno lo stato locale dell'asta
            print("Starting new auction")
            auction_state["is_active"] = True
            auction_state["highest_bid"] = 0
            auction_state["winner_id"] = -1
            auction_state["sequence_number"] = 0
            auction_state["bid_history"] = []

            # Genera automaticamente un auction_id incrementale
            auction_id = get_next_sequence_value("auction_id")
            # Inserisci l'asta nel database
            new_auction = {
                "auction_id": auction_id,
                "is_active": True,
                "highest_bid": 0,
                "winner_id": -1,
                "sequence_number": 0,
                "bid_history": []
            }
            auction_collection.insert_one(new_auction)

        elif message.message_type == "end":
            print(f"Ending auction. Winner: {message.winner_id}, Final bid: {message.highest_bid}")
            # Aggiorno lo stato locale dell'asta
            auction_state["is_active"] = False
            auction_state["winner_id"] = message.winner_id
            auction_state["highest_bid"] = message.highest_bid

            # Termina l'asta attiva nel database
            auction = auction_collection.find_one({"is_active": True})
            if not auction:
                raise HTTPException(status_code=404, detail="No active auction found")

            auction_collection.update_one(
                {"auction_id": auction["auction_id"]},
                {"$set": {"is_active": False}}
            )

        response = {"status": "message received", "current_state": auction_state}
        print(f"Sending response: {response}")
        return response

    except Exception as e:
        print(f"Error processing message: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))
    
@app.get("/stats-page")
async def stats_page(request: Request):
    print("Received request for dashboard page")
    return templates.TemplateResponse("stats.html",{"request": request})

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

@app.get("/all_auctions")
async def get_all_auctions():
    """
    Restituisce tutte le aste nel database.
    """
    auctions = list(auction_collection.find({}, {"_id": 0}))
    return {"auctions": auctions}

@app.get("/auction_stats")
async def auction_statistics():
    try:
        # Offerta vincente media
        avg_winning_bid = auction_collection.aggregate([
            {"$match": {"is_active": False}},  # Solo aste concluse
            {"$group": {"_id": None, "average_winning_bid": {"$avg": "$highest_bid"}}}
        ])
        avg_winning_bid = list(avg_winning_bid)
        avg_winning_bid = avg_winning_bid[0]["average_winning_bid"] if avg_winning_bid else 0

        # Numero medio di offerte per asta
        avg_bids_per_auction = auction_collection.aggregate([
            {"$match": {"is_active": False}},  # Solo aste concluse
            {"$project": {"num_bids": {"$size": "$bid_history"}}},  # Conta le offerte
            {"$group": {"_id": None, "average_bids": {"$avg": "$num_bids"}}}
        ])
        avg_bids_per_auction = list(avg_bids_per_auction)
        avg_bids_per_auction = avg_bids_per_auction[0]["average_bids"] if avg_bids_per_auction else 0

        # Offerta vincente più alta e più bassa
        winning_bids = auction_collection.aggregate([
            {"$match": {"is_active": False}},  # Solo aste concluse
            {"$group": {
                "_id": None,
                "max_winning_bid": {"$max": "$highest_bid"},
                "min_winning_bid": {"$min": "$highest_bid"}
            }}
        ])
        winning_bids = list(winning_bids)
        max_winning_bid = winning_bids[0]["max_winning_bid"] if winning_bids else 0
        min_winning_bid = winning_bids[0]["min_winning_bid"] if winning_bids else 0

        # Numero totale di aste concluse e attive
        total_active = auction_collection.count_documents({"is_active": True})
        total_concluded = auction_collection.count_documents({"is_active": False})

        return {
            "average_winning_bid": avg_winning_bid,
            "average_bids_per_auction": avg_bids_per_auction,
            "max_winning_bid": max_winning_bid,
            "min_winning_bid": min_winning_bid,
            "total_active_auctions": total_active,
            "total_concluded_auctions": total_concluded
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/", response_class=HTMLResponse)
async def get_dashboard(request: Request):
    print("Received request for dashboard page")
    return templates.TemplateResponse("home.html", {"request": request})

if __name__ == "__main__":
    print("Starting server on http://0.0.0.0:8000")
    uvicorn.run(app, host="0.0.0.0", port=8000)