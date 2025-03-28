#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QVector>
#include <unordered_map>
#include <algorithm>
#include <numeric>

class [[nodiscard]] Utils final {  // Mark as final if not meant to be inherited
// Likely/unlikely macros for branch prediction
#if defined(__GNUC__) || defined(__clang__)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
public:
    [[nodiscard]] static QVector<QString> splitAndNormalize(const QString& text) noexcept {
        QVector<QString> result;
        if (text.isEmpty()) return result;

        // More precise capacity estimation
        const int spaceCount = std::count(text.begin(), text.end(), ' ');
        result.reserve(spaceCount + 1);  // At least as many words as spaces + 1

        QString current;
        current.reserve(32);  // Keep reasonable word length

        const QChar* data = text.constData();
        const int length = text.length();

        for (int i = 0; i < length; ++i) {
            const QChar& ch = data[i];
            if (isFilteredChar(ch)) {
                if (!current.isEmpty()) {
                    result.emplace_back(std::move(current));
                    current.clear();
                }
            } else {
                current.append(ch);
            }
        }

        if (!current.isEmpty()) {
            result.emplace_back(std::move(current));
        }

        // Only shrink if significantly over-allocated
        if (result.capacity() > result.size() * 2) {
            result.shrink_to_fit();
        }
        return result;
    }

    [[nodiscard]] static QString mergeStrings(const QString& A, const QString& B) noexcept {
        if (A.isEmpty()) return B;
        if (B.isEmpty()) return A;

        const QVector<QString> base = splitAndNormalize(A);
        const QVector<QString> tail = splitAndNormalize(B);

        if (base.isEmpty()) return B;
        if (tail.isEmpty()) return A;

        // Optimized hash function with better mixing
        auto computeTrigramHash = [](const QString& a, const QString& b, const QString& c) noexcept {
            uint64_t h1 = qHash(a);
            uint64_t h2 = qHash(b);
            uint64_t h3 = qHash(c);
            return ((h1 * 0xFEA5B) ^ (h2 * 0x8DA6B) ^ (h3 * 0x7A97C)) * 0x9E3779B9;
        };

        // Build trigram index with view-based optimization
        std::unordered_map<uint64_t, QVector<int>> trigramIndex;
        trigramIndex.reserve(std::max(0, static_cast<int>(base.size() - 2)));

        for (int i = 0; i < base.size() - 2; ++i) {
            if (likely(!base[i].isEmpty() && !base[i+1].isEmpty() && !base[i+2].isEmpty())) {
                trigramIndex[computeTrigramHash(base[i], base[i+1], base[i+2])].push_back(i);
            }
        }

        // Optimized matching with early exit
        int bestMatchIndex = findBestMatch(tail, base, trigramIndex);

        // Calculate exact required size
        const int totalSize = calculateResultSize(A, B, base, tail, bestMatchIndex);
        QString result;
        result.reserve(totalSize);

        if (bestMatchIndex > 0) {
            result = base[0];
            for (int i = 1; i < bestMatchIndex; ++i) {
                result += ' ';
                result += base[i];
            }
            result += ' ';
        } else if (bestMatchIndex < 0) {
            result = A;
            if (!result.endsWith(' ')) result += ' ';
        }

        result += B;
        return result;
    }

private:
    [[nodiscard]] static constexpr bool isFilteredChar(const QChar& ch) noexcept {
        const ushort uc = ch.unicode();
        // Branchless version - may be faster on some architectures
        return (uc == ' ') | (uc == ',') | (uc == '.') | (uc == ';') | (uc == '-');
    }

    [[nodiscard]] static int findBestMatch(const QVector<QString>& tail,
                                           const QVector<QString>& base,
                                           const std::unordered_map<uint64_t, QVector<int>>& trigramIndex) noexcept {
        for (int i = 0; i < tail.size() - 2; ++i) {
            if (unlikely(tail[i].isEmpty() || tail[i+1].isEmpty() || tail[i+2].isEmpty())) {
                continue;
            }

            // Optimized hash function with better mixing
            auto computeTrigramHash = [](const QString& a, const QString& b, const QString& c) noexcept {
                uint64_t h1 = qHash(a);
                uint64_t h2 = qHash(b);
                uint64_t h3 = qHash(c);
                return ((h1 * 0xFEA5B) ^ (h2 * 0x8DA6B) ^ (h3 * 0x7A97C)) * 0x9E3779B9;
            };

            const uint64_t tailHash = computeTrigramHash(tail[i], tail[i+1], tail[i+2]);
            if (const auto it = trigramIndex.find(tailHash); it != trigramIndex.end()) {
                for (int baseIndex : it->second) {
                    if (baseIndex + 2 < base.size() &&
                        tail[i] == base[baseIndex] &&
                        tail[i+1] == base[baseIndex+1] &&
                        tail[i+2] == base[baseIndex+2]) {
                        return baseIndex;
                    }
                }
            }
        }
        return -1;
    }

    [[nodiscard]] static int calculateResultSize(const QString& A, const QString& B,
                                                 const QVector<QString>& base,
                                                 const QVector<QString>& tail,
                                                 int bestMatchIndex) noexcept {
        if (bestMatchIndex > 0) {
            int size = std::accumulate(base.begin(), base.begin() + bestMatchIndex, 0,
                                       [](int sum, const QString& s) { return sum + s.size(); });
            return size + bestMatchIndex - 1 + 1 + B.size();  // words + spaces + space + B
        }
        return A.size() + (A.endsWith(' ') ? 0 : 1) + B.size();
    }
};

#endif
