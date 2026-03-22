# ─────────────────────────────────────────────────────────────────────────────
# LowLatencyExchangeEngine — Makefile
#
# CMake'i saran ince bir wrapper. Proje kökü temiz kalır;
# tüm üretilen dosyalar build/ veya build_release/ içine gider.
#
# Hedefler:
#   make            → Debug build (varsayılan)
#   make test       → Debug build + testleri çalıştır
#   make release    → Release build (optimizasyonlu, assert kapalı)
#   make bench      → Release build + benchmark'ları çalıştır
#   make clean      → build dizinlerini tamamen sil
#   make rebuild    → clean + Debug build
# ─────────────────────────────────────────────────────────────────────────────

BUILD_DIR   := build
RELEASE_DIR := build_release

.PHONY: all build test bench release clean rebuild

# ── Varsayılan hedef ──────────────────────────────────────────────────────────

all: build

# ── Debug build ───────────────────────────────────────────────────────────────

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --parallel

# ── Testler ───────────────────────────────────────────────────────────────────

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

# ── Release build ─────────────────────────────────────────────────────────────

release:
	cmake -S . -B $(RELEASE_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(RELEASE_DIR) --parallel

# ── Benchmark'lar (Release üzerinde çalıştır) ─────────────────────────────────

bench: release
	$(RELEASE_DIR)/exchange_bench

# ── Temizlik ──────────────────────────────────────────────────────────────────

clean:
	rm -rf $(BUILD_DIR) $(RELEASE_DIR)

# ── Sıfırdan Debug build ──────────────────────────────────────────────────────

rebuild: clean build
