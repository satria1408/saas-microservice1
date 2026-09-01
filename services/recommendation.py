from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.metrics.pairwise import cosine_similarity
from services.book_catalog import get_all_books


def get_recommendations(judul_acuan: str, top_n: int = 3) -> list[dict]:
    books = get_all_books()

    if len(books) < 2:
        return []

    acuan_idx = None
    for i, b in enumerate(books):
        if b["judul"].strip().lower() == judul_acuan.strip().lower():
            acuan_idx = i
            break

    if acuan_idx is None:
        return []

    texts = [b.get("penjelasan") or "" for b in books]

    vectorizer = TfidfVectorizer(stop_words=None)
    tfidf_matrix = vectorizer.fit_transform(texts)

    similarity_scores = cosine_similarity(tfidf_matrix[acuan_idx], tfidf_matrix)[0]

    ranked_indices = similarity_scores.argsort()[::-1]
    ranked_indices = [i for i in ranked_indices if i != acuan_idx]

    hasil = []
    for i in ranked_indices[:top_n]:
        if similarity_scores[i] <= 0:
            continue
        hasil.append({
            "judul": books[i]["judul"],
            "penulis": books[i]["penulis"],
            "skor_kemiripan": round(float(similarity_scores[i]), 3),
        })

    return hasil