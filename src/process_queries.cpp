#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries) {
    vector<vector<Document>> final_documents(queries.size());
    transform(execution::par, queries.begin(), queries.end(), final_documents.begin(),
        [&search_server](string query) {
            return search_server.FindTopDocuments(query);
        });
    return final_documents;
}
vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries) {
    vector<Document> result;
    for (const vector<Document>& query_document : ProcessQueries(search_server, queries)) {
        for (const Document& document : query_document) {
            result.push_back(document);
        }
    }
    return result;
}