#pragma once
#include <iostream>
#include "document.h"
#include <vector>
#include <set>
#include <map>
#include <string>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <execution>
#include <string_view>
#include <deque>
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

template <typename StringContainer>
std::set<std::string_view> MakeUniqueNonEmptyStrings(const StringContainer& strings);

class SearchServer {

public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view& raw_query,
        DocumentPredicate document_predicate) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view& raw_query,
        DocumentStatus status) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view& raw_query) const;

    int GetDocumentCount() const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query,
        int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&,
        const std::string_view& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&,
        const std::string_view& raw_query, int document_id) const;

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

private:

    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::set<std::string, std::less<>> stop_words_;
    std::deque<std::string> all_words_in_documents;

    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_id_count_;
    std::map<int, std::map<std::string_view, double>> id_to_document_freqs_;

    bool IsStopWord(const std::string_view& word) const;
    static bool IsValidWord(const std::string_view& word);
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    QueryWord ParseQueryWord(std::string_view& text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view& text, bool sort_words = true) const;
    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query,
        DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query,
        DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
std::set<std::string_view> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string_view> non_empty_strings;
    for (auto& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
{
    std::set<std::string_view> vec = MakeUniqueNonEmptyStrings(stop_words);
    for (auto& word : vec) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Invalid stop word");
        }
        stop_words_.insert(std::string(word));
    }
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
    const std::string_view& raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
    const std::string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy,
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query,
    DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
    const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    std::sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&,
    const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (const std::string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }
    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&,
    const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(1000);
    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
        [&](const std::string_view& word) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        });
    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
        [&](const std::string_view& word) {
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        });
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}