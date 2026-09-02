from fastapi import Header, HTTPException
from config import LOCAL_API_KEY


def verify_local_api_key(x_api_key: str = Header(default=None)):
    if x_api_key != LOCAL_API_KEY:
        raise HTTPException(status_code=401, detail="API key tidak valid atau tidak disertakan")