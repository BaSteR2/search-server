#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {

    set<int> duplicates_for_remove;
    map<set<string_view>, int> unique_documents;

    for (const int document_id : search_server) {

        set<string_view> current_document_;
        for (auto& [word, freqs] : search_server.GetWordFrequencies(document_id)) {
            current_document_.insert(word);
        }
        if (unique_documents.count(current_document_)) {
            duplicates_for_remove.insert(document_id);
        }
        else {
            unique_documents.insert({ current_document_, document_id });
        }
    }

    for (int duplicate : duplicates_for_remove) {
        cout << "Found duplicate document id "s << duplicate << endl;
        search_server.RemoveDocument(duplicate);
    }
}
