from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from pymongo import MongoClient
from bson.objectid import ObjectId
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

# Stato iniziale (usato solo per testing/debug)
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

app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")

@app.post("/receive-data")
async def receive_data(message: AuctionMessage):
    try:
        if message.message_type == "start":
            # Genera automaticamente un auction_id incrementale
            auction_id = get_next_sequence_value("auction_id")
            new_auction = {
                "auction_id": auction_id,
                "is_active": True,
                "highest_bid": 0,
                "winner_id": -1,
                "sequence_number": 0,
                "bid_history": []
            }
            auction_collection.insert_one(new_auction)
            return {"message": "Auction started", "auction_id": auction_id}

        elif message.message_type == "order":
            # Trova l'asta attiva
            auction = auction_collection.find_one({"is_active": True})
            if not auction:
                raise HTTPException(status_code=404, detail="No active auction found")

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
            return {"message": "Bid received", "auction_id": auction["auction_id"], "current_state": updated_bid}

        elif message.message_type == "end":
            # Termina l'asta attiva
            auction = auction_collection.find_one({"is_active": True})
            if not auction:
                raise HTTPException(status_code=404, detail="No active auction found")

            auction_collection.update_one(
                {"auction_id": auction["auction_id"]},
                {"$set": {"is_active": False}}
            )
            return {"message": "Auction ended", "auction_id": auction["auction_id"]}

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/auction_state")
async def get_auction_state():
    """
    Restituisce lo stato dell'asta attiva.
    """
    auction = auction_collection.find_one({"is_active": True})
    if auction:
        return auction
    return {"message": "No active auction"}

@app.get("/bids_history")
async def get_bid_history():
    """
    Restituisce lo storico delle offerte dell'asta attiva.
    """
    auction = auction_collection.find_one({"is_active": True})
    if auction:
        return auction.get("bid_history", [])
    return {"message": "No active auction"}

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

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
