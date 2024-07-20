#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server), no_rezult_request_(0) {}

vector<Document> RequestQueue::AddFindRequest(const string_view& raw_query, DocumentStatus status) {
    return AddFindRequest(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> RequestQueue::AddFindRequest(const string_view& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
    return no_rezult_request_;
}

void RequestQueue::AddNewRequest(vector<Document>& document) {
    current_number_request_++;
    if (document.empty()) {
        requests_.push_back({ current_number_request_, false });
        no_rezult_request_++;
    }
    else {
        requests_.push_back({ current_number_request_, true });
    }
}