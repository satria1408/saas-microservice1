from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.metrics.pairwise import cosine_similarity
from services.book_catalog import get_all_books

# Bonus skor kalau kategori sama - angka ini hasil kompromi, bisa
# disesuaikan lagi kalau hasil rekomendasi masih kurang relevan
BONUS_KATEGORI_SAMA = 0.2


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

    kategori_acuan = books[acuan_idx].get("kategori")

    texts = [b.get("penjelasan") or "" for b in books]

    if not any(t.strip() for t in texts):
        return []

    vectorizer = TfidfVectorizer(stop_words=None)
    try:
        tfidf_matrix = vectorizer.fit_transform(texts)
    except ValueError:
        return []

    similarity_scores = cosine_similarity(tfidf_matrix[acuan_idx], tfidf_matrix)[0]

    # Tambahkan bonus skor untuk buku dengan kategori yang sama - ini
    # membantu kasus di mana TF-IDF murni gagal menangkap kemiripan tema
    # (contoh: 1984 vs buku self-help yang kebetulan pakai kata mirip)
    final_scores = []
    for i, score in enumerate(similarity_scores):
        bonus = 0.0
        if i != acuan_idx and kategori_acuan and books[i].get("kategori") == kategori_acuan:
            bonus = BONUS_KATEGORI_SAMA
        final_scores.append(score + bonus)

    ranked_indices = sorted(
        range(len(final_scores)), key=lambda i: final_scores[i], reverse=True
    )
    ranked_indices = [i for i in ranked_indices if i != acuan_idx]

    hasil = []
    for i in ranked_indices[:top_n]:
        if final_scores[i] <= 0:
            continue
        hasil.append({
            "judul": books[i]["judul"],
            "penulis": books[i]["penulis"],
            "kategori": books[i].get("kategori"),
            "skor_kemiripan": round(float(final_scores[i]), 3),
            "kategori_sama": books[i].get("kategori") == kategori_acuan if kategori_acuan else False,
        })

    return hasil