#pragma once

#include <deque>
#include "search_server.h"
#include "document.h"

class RequestQueue {
public:

    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string_view& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string_view& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string_view& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        int rezult;
        bool document_status;
    };
    std::deque<QueryResult> requests_;
    const SearchServer& search_server_;
    const static int min_in_day_ = 1440;
    int no_rezult_request_;
    int current_number_request_ = 0;
    void AddNewRequest(std::vector<Document>& document);

};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string_view& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> doc = search_server_.FindTopDocuments(raw_query, document_predicate);
    if (requests_.size() == min_in_day_) {
        requests_.pop_front();
        no_rezult_request_--;
        AddNewRequest(doc);
    }
    else {
        AddNewRequest(doc);
    }
    return doc;
}