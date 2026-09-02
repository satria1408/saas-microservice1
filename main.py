from fastapi import FastAPI
from routers import scan
from services.book_catalog import init_catalog_db
from routers import scan, rekomendasi
from fastapi import FastAPI, Depends
from routers import scan, rekomendasi
from services.book_catalog import init_catalog_db
from services.auth import verify_local_api_key

app = FastAPI(title="Book Scanner API")

@app.on_event("startup")
def startup():
    init_catalog_db()  

app.include_router(scan.router)
app.include_router(rekomendasi.router)
