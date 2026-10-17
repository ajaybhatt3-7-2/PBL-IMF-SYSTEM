#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cctype>
#include <cstdio>
#include <cstdlib>

using namespace std;

const int    MAX_DEPARTMENTS   = 100;         
const size_t MAX_NAME_LENGTH   = 30;
const size_t MAX_DESC_LENGTH   = 80;
const size_t MAX_EMP_ID_LENGTH = 15;
const char   FIELD_SEPARATOR   = '|';         
const string DATA_FILE         = "departments.txt";
const string TEMP_FILE         = "departments.tmp";
const string NO_PARENT         = "NONE";      

struct Department {
    string departmentId;         
    string name;                 
    string description;          
    string managerEmployeeId;    
    string parentDepartmentId;   
};

Department departments[MAX_DEPARTMENTS];
int departmentCount = 0;

string trim(const string& text) {
    size_t start = 0;
    while (start < text.size() && isspace((unsigned char)text[start])) start++;
    size_t end = text.size();
    while (end > start && isspace((unsigned char)text[end - 1])) end--;
    return text.substr(start, end - start);
}

string toUpperCase(string text) {
    for (size_t i = 0; i < text.size(); i++)
        text[i] = (char)toupper((unsigned char)text[i]);
    return text;
}

string readLine(const string& prompt) {
    string line;
    cout << prompt;
    if (!getline(cin, line)) {
        cout << "\nInput closed. Exiting program.\n";
        exit(0);
    }
    return trim(line);
}

string readText(const string& prompt, size_t maxLength, bool allowEmpty) {
    while (true) {
        string text = readLine(prompt);
        if (text.empty()) {
            if (allowEmpty) return text;
            cout << "Error: this field cannot be empty.\n";
            continue;
        }
        if (text.size() > maxLength) {
            cout << "Error: maximum " << maxLength << " characters allowed.\n";
            continue;
        }
        if (text.find(FIELD_SEPARATOR) != string::npos) {
            cout << "Error: the '|' character is not allowed.\n";
            continue;
        }
        return text;
    }
}

bool isValidDepartmentIdFormat(const string& id) {
    if (id.size() != 4 || id[0] != 'D') return false;
    for (int i = 1; i < 4; i++)
        if (!isdigit((unsigned char)id[i])) return false;
    return true;
}

bool isValidEmployeeIdFormat(const string& id) {
    if (id.empty() || id.size() > MAX_EMP_ID_LENGTH) return false;
    for (size_t i = 0; i < id.size(); i++) {
        unsigned char c = (unsigned char)id[i];
        if (!isalnum(c) && c != '-' && c != '_') return false;
    }
    return true;
}

bool employeeExists(const string& id) {
    (void)id;       
    return true;
}
int findDepartmentIndexById(const string& id) {
    for (int i = 0; i < departmentCount; i++)
        if (departments[i].departmentId == id) return i;
    return -1;
}
bool isDepartmentNameTaken(const string& name, int ignoreIndex) {
    string wanted = toUpperCase(name);
    for (int i = 0; i < departmentCount; i++) {
        if (i == ignoreIndex) continue;
        if (toUpperCase(departments[i].name) == wanted) return true;
    }
    return false;
}

string generateNextDepartmentId() {
    for (int n = 1; n <= 999; n++) {
        ostringstream oss;
        oss << "D" << setw(3) << setfill('0') << n;
        if (findDepartmentIndexById(oss.str()) == -1) return oss.str();
    }
    return "";
}

string readManagerEmployeeId(const string& prompt) {
    while (true) {
        string id = toUpperCase(readLine(prompt));
        if (!isValidEmployeeIdFormat(id)) {
            cout << "Error: Employee ID must be 1-" << MAX_EMP_ID_LENGTH
                 << " characters (letters, digits, '-' or '_', no spaces).\n";
            continue;
        }
        if (!employeeExists(id)) {
            cout << "Error: no employee found with ID " << id << ".\n";
            continue;
        }
        return id;
    }
}

vector<string> splitLine(const string& line) {
    vector<string> fields;
    string current;
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == FIELD_SEPARATOR) {
            fields.push_back(current);
            current.clear();
        } else {
            current += line[i];
        }
    }
    fields.push_back(current);
    return fields;
}
bool saveDepartments() {
    ofstream out(TEMP_FILE.c_str());
    if (!out) {
        cout << "Error: could not create '" << TEMP_FILE << "'. Data NOT saved.\n";
        return false;
    }
    for (int i = 0; i < departmentCount; i++) {
        out << departments[i].departmentId << FIELD_SEPARATOR
            << departments[i].name << FIELD_SEPARATOR
            << departments[i].description << FIELD_SEPARATOR
            << departments[i].managerEmployeeId << FIELD_SEPARATOR
            << departments[i].parentDepartmentId << '\n';
    }
    out.close();
    if (out.fail()) {
        cout << "Error: writing to '" << TEMP_FILE << "' failed. Data NOT saved.\n";
        remove(TEMP_FILE.c_str());
        return false;
    }
    remove(DATA_FILE.c_str());   
    if (rename(TEMP_FILE.c_str(), DATA_FILE.c_str()) != 0) {
        cout << "Error: could not replace '" << DATA_FILE
             << "'. Latest data is in '" << TEMP_FILE << "'.\n";
        return false;
    }
    return true;
}
void loadDepartments() {
    ifstream in(DATA_FILE.c_str());
    if (!in) {
        cout << "No saved file found. Starting with an empty department list.\n";
        return;
    }

    string line;
    int lineNumber = 0, skipped = 0;
    while (getline(in, line)) {
        lineNumber++;
        if (trim(line).empty()) continue;

        vector<string> f = splitLine(line);
        if (f.size() != 5) {
            cout << "Warning: line " << lineNumber << " skipped (wrong field count).\n";
            skipped++;
            continue;
        }

        Department d;
        d.departmentId       = toUpperCase(trim(f[0]));
        d.name               = trim(f[1]);
        d.description        = trim(f[2]);
        d.managerEmployeeId  = toUpperCase(trim(f[3]));
        d.parentDepartmentId = toUpperCase(trim(f[4]));

        bool parentOk = (d.parentDepartmentId == NO_PARENT) ||
                        isValidDepartmentIdFormat(d.parentDepartmentId);

        if (!isValidDepartmentIdFormat(d.departmentId) || d.name.empty() ||
            !isValidEmployeeIdFormat(d.managerEmployeeId) || !parentOk) {
            cout << "Warning: line " << lineNumber << " skipped (invalid data).\n";
            skipped++;
            continue;
        }
        if (findDepartmentIndexById(d.departmentId) != -1) {
            cout << "Warning: line " << lineNumber << " skipped (duplicate ID "
                 << d.departmentId << ").\n";
            skipped++;
            continue;
        }
        if (departmentCount >= MAX_DEPARTMENTS) {
            cout << "Warning: capacity of " << MAX_DEPARTMENTS
                 << " reached; remaining lines ignored.\n";
            break;
        }
        departments[departmentCount++] = d;
    }
    in.close();

    cout << departmentCount << " department(s) loaded";
    if (skipped > 0) cout << ", " << skipped << " line(s) skipped";
    cout << ".\n";
}
void displayDepartmentDetails(const Department* dept) {
    cout << "\n--- Department Details ---\n";
    cout << "Department ID : " << dept->departmentId << "\n";
    cout << "Name          : " << dept->name << "\n";
    cout << "Description   : " << dept->description << "\n";
    cout << "Manager Emp ID: " << dept->managerEmployeeId << "\n";
    cout << "Parent Dept   : " << dept->parentDepartmentId << "\n";
}
void displayAllDepartments() {
    if (departmentCount == 0) {
        cout << "\nNo departments available.\n";
        return;
    }
    cout << "\n" << left
         << setw(8)  << "ID"
         << setw(32) << "Name"
         << setw(17) << "Manager Emp ID"
         << setw(8)  << "Parent" << "\n";
    cout << string(65, '-') << "\n";
    for (int i = 0; i < departmentCount; i++) {
        cout << left
             << setw(8)  << departments[i].departmentId
             << setw(32) << departments[i].name
             << setw(17) << departments[i].managerEmployeeId
             << setw(8)  << departments[i].parentDepartmentId << "\n";
    }
    cout << "Total departments: " << departmentCount << "\n";
}

void addDepartment() {
    if (departmentCount >= MAX_DEPARTMENTS) {
        cout << "Error: department list is full (" << MAX_DEPARTMENTS << ").\n";
        return;
    }

    Department d;

    while (true) {
        string input = toUpperCase(readLine(
            "Enter Department ID (format D001) or press Enter to auto-generate: "));
        if (input.empty()) {
            input = generateNextDepartmentId();
            if (input.empty()) {
                cout << "Error: no free Department ID available.\n";
                continue;
            }
            cout << "Generated Department ID: " << input << "\n";
        } else if (!isValidDepartmentIdFormat(input)) {
            cout << "Error: ID must be the letter D followed by 3 digits (e.g. D001).\n";
            continue;
        }
        if (findDepartmentIndexById(input) != -1) {
            cout << "Error: Department ID " << input << " already exists.\n";
            continue;
        }
        d.departmentId = input;
        break;
    }

    while (true) {
        d.name = readText("Enter Department name: ", MAX_NAME_LENGTH, false);
        if (isDepartmentNameTaken(d.name, -1)) {
            cout << "Error: a department with this name already exists.\n";
            continue;
        }
        break;
    }

    d.description = readText("Enter description (optional, press Enter to skip): ",
                             MAX_DESC_LENGTH, true);
    if (d.description.empty()) d.description = "N/A";

    d.managerEmployeeId  = readManagerEmployeeId("Enter Manager Employee ID: ");
    d.parentDepartmentId = NO_PARENT;  

    departments[departmentCount++] = d;
    cout << "Department " << d.departmentId << " added successfully.\n";
    if (saveDepartments()) cout << "Data saved to " << DATA_FILE << ".\n";
}

void searchDepartmentById() {
    if (departmentCount == 0) {
        cout << "\nNo departments available to search.\n";
        return;
    }
    string id = toUpperCase(readLine("Enter Department ID to search: "));
    if (!isValidDepartmentIdFormat(id)) {
        cout << "Error: invalid Department ID format (expected e.g. D001).\n";
        return;
    }
    int index = findDepartmentIndexById(id);
    if (index == -1) {
        cout << "Department " << id << " not found.\n";
        return;
    }
    const Department* found = &departments[index];  
    displayDepartmentDetails(found);
}

void updateDepartment() {
    if (departmentCount == 0) {
        cout << "\nNo departments available to update.\n";
        return;
    }
    string id = toUpperCase(readLine("Enter Department ID to update: "));
    if (!isValidDepartmentIdFormat(id)) {
        cout << "Error: invalid Department ID format (expected e.g. D001).\n";
        return;
    }
    int index = findDepartmentIndexById(id);
    if (index == -1) {
        cout << "Department " << id << " not found.\n";
        return;
    }

    Department* dept = &departments[index];
    displayDepartmentDetails(dept);

    cout << "\nWhat do you want to update?\n"
         << "  1. Name\n  2. Description\n  3. Manager Employee ID\n  0. Cancel\n";
    string choice = readLine("Enter choice: ");

    if (choice == "1") {
        while (true) {
            string newName = readText("Enter new name: ", MAX_NAME_LENGTH, false);
            if (isDepartmentNameTaken(newName, index)) {
                cout << "Error: a department with this name already exists.\n";
                continue;
            }
            dept->name = newName;
            break;
        }
    } else if (choice == "2") {
        string newDesc = readText("Enter new description (Enter for none): ",
                                  MAX_DESC_LENGTH, true);
        dept->description = newDesc.empty() ? "N/A" : newDesc;
    } else if (choice == "3") {
        dept->managerEmployeeId = readManagerEmployeeId("Enter new Manager Employee ID: ");
    } else if (choice == "0") {
        cout << "Update cancelled.\n";
        return;
    } else {
        cout << "Invalid choice. Nothing changed.\n";
        return;
    }

    cout << "Department " << dept->departmentId << " updated.\n";
    if (saveDepartments()) cout << "Data saved to " << DATA_FILE << ".\n";
}

void showMenu() {
    cout << "\n=========== IMF - Department Management ===========\n"
         << "  1. Add department\n"
         << "  2. Display all departments\n"
         << "  3. Search department by ID\n"
         << "  4. Update department (name / description / manager)\n"
         << "  5. Save data to file now\n"
         << "  0. Save and exit\n"
         << "===================================================\n";
}

int main() {
    loadDepartments();

    while (true) {
        showMenu();
        string choice = readLine("Enter your choice: ");

        if (choice == "1") {
            addDepartment();
        } else if (choice == "2") {
            displayAllDepartments();
        } else if (choice == "3") {
            searchDepartmentById();
        } else if (choice == "4") {
            updateDepartment();
        } else if (choice == "5") {
            if (saveDepartments()) cout << "Data saved to " << DATA_FILE << ".\n";
        } else if (choice == "0") {
            if (saveDepartments()) cout << "Data saved. Goodbye!\n";
            else cout << "Exiting (save failed, see message above).\n";
            break;
        } else {
            cout << "Invalid choice. Please enter a number from the menu.\n";
        }
    }
    return 0;
}
