#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

const string DATA_FILE = "leave_requests.txt";
const char* TYPES[] = {"Casual Leave", "Sick Leave", "Earned Leave", "Other Leave"};

struct LeaveRequest { int id; string empId, type, startDate, endDate, reason, status; };

string trim(const string& s) {
    size_t b = 0, e = s.size();
    while (b < e && isspace((unsigned char)s[b])) b++;
    while (e > b && isspace((unsigned char)s[e - 1])) e--;
    return s.substr(b, e - b);
}
bool readLine(const string& prompt, string& out) {
    cout << prompt;
    if (!getline(cin, out)) return false;
    out = trim(out);
    return true;
}
bool ask(const string& prompt, string& out, bool (*valid)(const string&), const string& err) {
    while (readLine(prompt, out)) {
        if (valid(out)) return true;
        cout << err << "\n";
    }
    return false;
}
int choice(const string& s) {
    if (s.empty() || s.size() > 3) return -1;
    for (char c : s) if (!isdigit((unsigned char)c)) return -1;
    return stoi(s);
}
string fmtId(int id) { ostringstream o; o << "LR" << setw(4) << setfill('0') << id; return o.str(); }

bool isValidEmpId(const string& s) {
    if (s.size() < 3 || s.size() > 15) return false;
    for (char c : s) if (!isalnum((unsigned char)c)) return false;
    return true;
}

bool isValidDate(const string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i = 0; i < 10; i++)
        if (i != 4 && i != 7 && !isdigit((unsigned char)s[i])) return false;
    int y = stoi(s.substr(0, 4)), m = stoi(s.substr(5, 2)), d = stoi(s.substr(8, 2));
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1) return false;
    int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return d <= dim[m - 1] + (m == 2 && leap);
}

bool isValidReason(const string& s) { return !s.empty() && s.size() <= 200; }
bool isType(const string& t) { for (auto x : TYPES) if (t == x) return true; return false; }
bool isStatus(const string& s) { return s == "Pending" || s == "Approved" || s == "Rejected"; }

bool save(const vector<LeaveRequest>& v) {
    ofstream out(DATA_FILE, ios::trunc);
    if (!out) return false;
    for (auto& r : v)
        out << fmtId(r.id) << '|' << r.empId << '|' << r.type << '|' << r.startDate << '|'
            << r.endDate << '|' << r.reason << '|' << r.status << '\n';
    out.flush();
    return out.good();
}

void load(vector<LeaveRequest>& v) {
    ifstream in(DATA_FILE);
    if (!in) { cout << "No saved file found. Starting empty.\n"; return; }
    string line, field;
    int bad = 0;
    while (getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        vector<string> f;
        istringstream ss(line);
        while (getline(ss, field, '|')) f.push_back(field);

        bool ok = f.size() == 7 && f[0].size() > 2 && f[0].size() <= 10 && f[0].substr(0, 2) == "LR";
        for (size_t i = 2; ok && i < f[0].size(); i++) ok = isdigit((unsigned char)f[0][i]);
        LeaveRequest r;
        if (ok) {
            r = {stoi(f[0].substr(2)), f[1], f[2], f[3], f[4], f[5], f[6]};
            ok = r.id > 0 && isValidEmpId(r.empId) && isType(r.type) && isValidDate(r.startDate) &&
                 isValidDate(r.endDate) && r.endDate >= r.startDate && !r.reason.empty() && isStatus(r.status);
            for (auto& e : v) if (e.id == r.id) ok = false;
        }
        if (ok) v.push_back(r); else bad++;
    }
    cout << "Loaded " << v.size() << " request(s).\n";
    if (bad) cout << "Warning: skipped " << bad << " invalid line(s).\n";
}

void submit(vector<LeaveRequest>& v) {
    cout << "\n--- Submit Leave Request ---\n";
    LeaveRequest r;
    string in;
    if (!ask("Employee ID (3-15 letters/digits): ", r.empId, isValidEmpId, "Invalid Employee ID.")) return;
    for (char& c : r.empId) c = (char)toupper((unsigned char)c);

    int t;
    do {
        cout << "1. Casual  2. Sick  3. Earned  4. Other\n";
        if (!readLine("Leave type (1-4): ", in)) return;
        t = choice(in);
    } while (t < 1 || t > 4);
    r.type = TYPES[t - 1];

    if (!ask("Start date (YYYY-MM-DD): ", r.startDate, isValidDate, "Invalid date.")) return;
    do {
        if (!ask("End date   (YYYY-MM-DD): ", r.endDate, isValidDate, "Invalid date.")) return;
        if (r.endDate < r.startDate) cout << "End date cannot be earlier than start date.\n";
    } while (r.endDate < r.startDate);

    if (!ask("Reason: ", r.reason, isValidReason, "Reason must be 1-200 characters.")) return;
    for (char& c : r.reason) if (c == '|') c = '/';

    r.id = 1;
    for (auto& e : v) if (e.id >= r.id) r.id = e.id + 1;
    r.status = "Pending";
    v.push_back(r);
    cout << "Request " << fmtId(r.id) << (save(v) ? " submitted and saved. Status: Pending.\n"
                                                  : " recorded, but SAVING FAILED.\n");
}

int show(const vector<LeaveRequest>& v, const string& emp = "") {
    int n = 0;
    for (auto& r : v) {
        if (!emp.empty() && r.empId != emp) continue;
        if (n++ == 0)
            cout << left << setw(8) << "ID" << setw(17) << "Employee" << setw(14) << "Type"
                 << setw(12) << "Start" << setw(12) << "End" << setw(10) << "Status" << "Reason\n";
        cout << left << setw(8) << fmtId(r.id) << setw(17) << r.empId << setw(14) << r.type
             << setw(12) << r.startDate << setw(12) << r.endDate << setw(10) << r.status
             << r.reason << "\n";
    }
    return n;
}

int main() {
    vector<LeaveRequest> requests;
    load(requests);
    string in;
    while (true) {
        cout << "\n=== IMF Leave Management (50%) ===\n1. Submit request\n2. Display all\n"
                "3. Search by Employee ID\n4. Exit\n";
        if (!readLine("Enter choice: ", in)) break;
        int c = choice(in);
        if (c == 1) submit(requests);
        else if (c == 2) { if (!show(requests)) cout << "No leave requests found.\n"; }
        else if (c == 3) {
            string emp;
            if (!ask("Employee ID: ", emp, isValidEmpId, "Invalid Employee ID.")) break;
            for (char& ch : emp) ch = (char)toupper((unsigned char)ch);
            if (!show(requests, emp)) cout << "No requests found for " << emp << ".\n";
        }
        else if (c == 4) { cout << "Goodbye.\n"; break; }
        else cout << "Invalid choice. Enter 1-4.\n";
    }
    return 0;
}
