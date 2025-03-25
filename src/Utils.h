#ifndef UTILS_H
#define UTILS_H

#include <qobject.h>
#include <QHash>
#include <QRegularExpression>

class Utils {
public:
    // Combined normalization and splitting in one pass
    static QVector<QString> splitAndNormalize(const QString &text) {
        QVector<QString> result;
        QString current;
        current.reserve(32); // Reserve typical word length

        for (const QChar& ch : text) {
            if (ch == ' ') {
                if (!current.isEmpty()) {
                    result.append(current);
                    current.clear();
                }
            }
            else if (ch != ',' && ch != '.' && ch != ';' && ch != '-') {
                current.append(ch);
            }
        }

        if (!current.isEmpty()) {
            result.append(current);
        }

        return result;
    }

    // Advanced optimized merge function
    static QString mergeStrings(const QString &A, const QString &B) {
        // Early exit if strings are empty
        if (A.isEmpty()) return B;
        if (B.isEmpty()) return A;

        QVector<QString> base = splitAndNormalize(A);
        QVector<QString> tail = splitAndNormalize(B);

        // Use a single hash map for trigram to position
        std::unordered_map<uint, QVector<int>> trigramIndex;
        const int baseSize = base.size();

        // Build trigram index using hashes instead of strings
        for (int i = 0; i < baseSize - 2; ++i) {
            if (base[i].length() >= 1 && base[i+1].length() >= 1 && base[i+2].length() >= 1) {
                uint hash = qHash(base[i]) ^ qHash(base[i+1]) ^ qHash(base[i+2]);
                trigramIndex[hash].append(i);
            }
        }

        int bestMatchIndex = -1;
        const int tailSize = tail.size();

        for (int i = 0; i <= tailSize - 3; ++i) {
            if (tail[i].length() < 1 || tail[i+1].length() < 1 || tail[i+2].length() < 1) {
                continue;
            }

            uint tailHash = qHash(tail[i]) ^ qHash(tail[i+1]) ^ qHash(tail[i+2]);
            auto trigramIt = trigramIndex.find(tailHash);
            if (trigramIt != trigramIndex.end()) {
                for (int baseIndex : trigramIt->second) {
                    if (baseIndex + 2 < baseSize &&
                        tail[i] == base[baseIndex] &&
                        tail[i+1] == base[baseIndex+1] &&
                        tail[i+2] == base[baseIndex+2]) {
                        bestMatchIndex = baseIndex;
                        break;
                    }
                }
                if (bestMatchIndex >= 0) break;
            }
        }

        // Construct merged result
        QString result;
        result.reserve(A.size() + B.size() + 1);

        if (bestMatchIndex > 0) {
            for (int i = 0; i < bestMatchIndex; ++i) {
                if (i != 0) result += ' ';
                result += base[i];
            }
            if (!tail.isEmpty()) result += ' ';
        } else if (bestMatchIndex < 0 && !base.isEmpty()) {
            result = base.join(' ');
            if (!tail.isEmpty()) result += ' ';
        }

        result += tail.join(' ');
        return result;
    }
};

#endif
