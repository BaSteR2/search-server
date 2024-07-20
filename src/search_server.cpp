#include "search_server.h"
#include "string_processing.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWordsView(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, const string_view& document,
    DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("Invalid ID document " + to_string(document_id));
    }
    if (documents_.count(document_id)) {
        throw invalid_argument("Document with ID " + to_string(document_id) + " already exist ");
    }
    const vector<string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string_view word : words) {
        all_words_in_documents.push_back(std::string(word));
        id_to_document_freqs_[document_id][all_words_in_documents.back()] += inv_word_count;
        word_to_document_freqs_[all_words_in_documents.back()][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_id_count_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq,
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query) const {
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    if (any_of(query.minus_words.begin(), query.minus_words.end(),
        [this, document_id](const string_view& word) {
            return word_to_document_freqs_.at(word).count(document_id);
        }))
    {
        return { std::vector<std::string_view>{}, documents_.at(document_id).status };
    }
        vector<string_view> matched_words;
        for (const string_view& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        return { matched_words, documents_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&,
    const string_view& raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&,
    const string_view& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query, false);
    if (any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
        [this, document_id](const string_view& word) {
            return word_to_document_freqs_.at(word).count(document_id);
        }))
    {
        return { std::vector<std::string_view>{}, documents_.at(document_id).status };
    }
        vector<string_view> matched_words(query.plus_words.size());
        auto last1 = copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(),
            [this, document_id](const string_view& word) {
                return word_to_document_freqs_.at(word).count(document_id);
            });
        matched_words.erase(last1, matched_words.end());
        std::sort(std::execution::par, matched_words.begin(), matched_words.end());
        auto last2 = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
        matched_words.erase(last2, matched_words.end());
        return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const string_view& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view& text) const {
    vector<string_view> words;
    for (const string_view& word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Invalid word in document: ");
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if ((text[0] == '-' && text[1] == '-') || text == "-" || !IsValidWord(text)) {
        throw invalid_argument("Invalid minus word ");
    }
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return { text, is_minus, IsStopWord(text) };
}


SearchServer::Query SearchServer::ParseQuery(const string_view& text, bool sort_words) const {
    Query result;
    for (string_view& word : SplitIntoWordsView(text)) {
        auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    if (sort_words) {
        sort(result.plus_words.begin(), result.plus_words.end());
        auto last_plus = unique(result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(last_plus, result.plus_words.end());

        sort(result.minus_words.begin(), result.minus_words.end());
        auto last_minus = unique(result.minus_words.begin(), result.minus_words.end());
        result.minus_words.erase(last_minus, result.minus_words.end());
    }

    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

set<int>::const_iterator SearchServer::begin()  const {
    return document_id_count_.begin();
}

set<int>::const_iterator SearchServer::end()  const {
    return document_id_count_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> empty_map;
    if (!document_id_count_.count(document_id)) {
        return empty_map;
    }
    return id_to_document_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    for (auto& [word, freqs] : GetWordFrequencies(document_id)) {
        word_to_document_freqs_[word].erase(document_id);
    }
    documents_.erase(document_id);
    id_to_document_freqs_.erase(document_id);
    document_id_count_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
    std::vector<const std::string_view*> words(id_to_document_freqs_.at(document_id).size());
    transform(std::execution::par,
        id_to_document_freqs_.at(document_id).begin(), id_to_document_freqs_.at(document_id).end(),
        words.begin(),
        [](auto& word) {
            return &word.first;
        });
    for_each(std::execution::par, words.begin(), words.end(), [&](const string_view* word) {
        word_to_document_freqs_[*word].erase(document_id);
        });
    documents_.erase(document_id);
    id_to_document_freqs_.erase(document_id);
    document_id_count_.erase(document_id);
}

