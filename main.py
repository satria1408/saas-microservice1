from fastapi import FastAPI
from routers import scan
from services.book_catalog import init_catalog_db
from routers import scan, rekomendasi

app = FastAPI(title="Book Scanner API")

@app.on_event("startup")
def startup():
    init_catalog_db()  

app.include_router(scan.router)
app.include_router(rekomendasi.router)
