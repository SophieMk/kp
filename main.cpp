#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;
using namespace std;

class OrdinalError : public runtime_error
{
public:
    OrdinalError(const string& what) : runtime_error(what) {}
};

struct Job {
    string id;
    string command;
    vector<string> ids_prev;
    vector<string> ids_next;
};

// https://en.wikipedia.org/wiki/Topological_sorting#Depth-first_search
class Toposorter {
    const map<string, Job>& id2job;
    bool is_used;
    set<string> marks_permanent;
    set<string> marks_temporary;
    vector<string> ids_ordered;

public:
    Toposorter(const map<string, Job>& id2job) : id2job(id2job), is_used(false)
    {}

    void visit(const string& id) {
        if (marks_permanent.find(id) != marks_permanent.end()) {
            return;
        }
        if (marks_temporary.find(id) != marks_temporary.end()) {
            throw OrdinalError("Not a DAG.");
        }

        marks_temporary.insert(id);

        const Job& job = id2job.find(id)->second;
        for (const string& id_next : job.ids_next) {
            visit(id_next);
        }

        marks_temporary.erase(id);
        marks_permanent.insert(id);
        ids_ordered.push_back(id);
    }

    vector<string> run()
    {
        assert(! is_used);
        is_used = true;

        // Обходим в обратном порядке из-за инверсии дальше (чтобы в итоге
        // сортировка была стабильной).
        for (auto it = id2job.rbegin(); it != id2job.rend(); it++) {
            const string& id = it->first;
            if (marks_permanent.find(id) == marks_permanent.end()) {
                visit(id);
            }
        }

        reverse(ids_ordered.begin(), ids_ordered.end());
        return ids_ordered;
    }
};

class Componenter {
    const map<string, Job>& id2job;
    bool is_used;
    set<string> ids_visited;

public:
    Componenter(const map<string, Job>& id2job) : id2job(id2job), is_used(false)
    {}

    void visit(const string& id) {
        if (ids_visited.find(id) != ids_visited.end()) {
            return;
        }
        ids_visited.insert(id);

        const Job& job = id2job.find(id)->second;
        for (const string& id_next : job.ids_next) {
            visit(id_next);
        }
        for (const string& id_prev : job.ids_prev) {
            visit(id_prev);
        }
    }

    void run()
    {
        assert(! is_used);
        is_used = true;

        assert(! id2job.empty());
        const string& id_first = id2job.begin()->first;
        visit(id_first);

        if (ids_visited.size() < id2job.size()) {
            throw OrdinalError("More than one component.");
        }
    }
};


vector<string> preprocess_dag(const map<string, Job>& id2job) {
    if (id2job.empty()) {
        throw OrdinalError("DAG is empty.");
    }

    vector<string> ids_ordered = Toposorter(id2job).run();
    Componenter(id2job).run();

    return ids_ordered;
}

map<string, Job> read_dag()
{
    json json_input;
    try {
        cin >> json_input;
    } catch (const nlohmann::detail::parse_error& exc) {
        throw OrdinalError(exc.what());
    }

    if (! json_input.is_object()) {
        throw OrdinalError("JSON input must be an object.");
    }

    map<string, Job> id2job;
    for (const auto& item : json_input.items()) {
        const string& id = item.key();
        try {
            id2job.emplace(
                id,
                Job{id, item.value()["command"], item.value()["deps"]}
            );
        } catch (const nlohmann::detail::type_error& exc) {
            throw OrdinalError("Job " + id + ": " + exc.what());
        }
    }

    for (auto& pair_id_job : id2job) {
        Job& job = pair_id_job.second;
        for (const string& id_prev : job.ids_prev) {
            const auto& it_id_job = id2job.find(id_prev);
            if (it_id_job == id2job.end()) {
                throw OrdinalError(
                    "Job " + job.id + ": dependency not found: " + id_prev
                );
            }
            Job& job_prev = it_id_job->second;
            job_prev.ids_next.push_back(job.id);
        }
    }

    return id2job;
}

void execute_dag(
    const map<string, Job>& id2job, const vector<string>& ids_ordered
) {
    for (const string& id : ids_ordered) {
        const auto& it_id_job = id2job.find(id);
        assert(it_id_job != id2job.end());
        const Job& job = it_id_job->second;

        int status = system(job.command.c_str());

        cerr << "Job " << id << ": " << job.command << ": ";
        bool is_ok = false;
        if (status == -1) {
            cerr << "system call failed";
        } else {
            int exitcode = WEXITSTATUS(status);
            cerr << "exit " << exitcode;
            is_ok = exitcode == 0;
        }
        cerr << '\n';

        if (! is_ok) {
            throw OrdinalError("A job failed.");
        }
    }
}

int main()
{
    int stage;
    try {
        stage = 1;
        map<string, Job> id2job = read_dag();

        stage = 2;
        vector<string> ids_ordered = preprocess_dag(id2job);

        stage = 3;
        execute_dag(id2job, ids_ordered);
    } catch (const OrdinalError& exc) {
        cerr << exc.what() << '\n';
        return stage;
    }
}
