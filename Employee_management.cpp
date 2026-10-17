#include <iostream>
#include <fstream>
#include <string>
using namespace std;
class Employee {
public:
    int id;
    string name;
    int age;
    string phone;
    string email;
    string designation;
    int salary;
    int deptId;
};
Employee emp[100];   
int total = 0;      


int findEmployee(int id) {
    for (int i = 0; i < total; i++) {
        if (emp[i].id == id) {
            return i;
        }
    }
    return -1;
}
void saveFile() {
    ofstream out("employees.txt");
    for (int i = 0; i < total; i++) {
        out << emp[i].id << "|" << emp[i].name << "|" << emp[i].age << "|" << emp[i].phone << "|" << emp[i].email << "|" << emp[i].designation << "|" << emp[i].salary << "|" << emp[i].deptId << "\n";
    }
}
void loadFile() {
    ifstream in("employees.txt");
    char sep;
    while (total < 100 && in >> emp[total].id) {
        in >> sep;
        getline(in, emp[total].name, '|');
        in >> emp[total].age >> sep;
        getline(in, emp[total].phone, '|');
        getline(in, emp[total].email, '|');
        getline(in, emp[total].designation, '|');
        in >> emp[total].salary >> sep >> emp[total].deptId;
        total++;
    }
}
void printEmployee(Employee e) {
    cout << "ID: " << e.id << "\tName: " << e.name << "\tAge: " << e.age << "\n";
    cout << "Phone: " << e.phone << "\tEmail: " << e.email << "\n";
    cout << "Designation: " << e.designation << "\tSalary: " << e.salary
         << "\tDept ID: " << e.deptId << "\n\n";
}
void readDetails(Employee &e) {
    cout << "Name : ";
    cin.ignore();
    getline(cin, e.name);
    cout << "Age: ";
    cin >> e.age;
    cout << "Phone: ";
    cin >> e.phone;
    cout << "Email: ";
    cin >> e.email;
    cout << "Designation : ";
    cin.ignore();
    getline(cin, e.designation);
    cout << "Salary: ";
    cin >> e.salary;
    cout << "Department ID: ";
    cin >> e.deptId;
}
void addEmployee() {
    if (total == 100) {
        cout << "List is full.\n";
        return;
    }
    Employee e;
    cout << "Employee ID: ";
    cin >> e.id;
    if (findEmployee(e.id) != -1) {
        cout << "This ID already exists.\n";
        return;
    }
    readDetails(e);
    emp[total] = e;
    total++;
    saveFile();
    cout << "Employee added.\n";
}
void displayAll() {
    if (total == 0) {
        cout << "No employees found.\n";
    }
    for (int i = 0; i < total; i++) {
        printEmployee(emp[i]);
    }
}
void searchById() {
    int id;
    cout << "Enter ID: ";
    cin >> id;
    int index = findEmployee(id);
    if (index == -1) {
        cout << "Employee not found.\n";
    } else {
        printEmployee(emp[index]);
    }
}
void searchByName() {
    string name;
    cout << "Enter name: ";
    cin.ignore();
    getline(cin,name);
    bool found = false;
    for (int i = 0; i < total; i++) {
        if (emp[i].name == name) {
            printEmployee(emp[i]);
            found = true;
        }
    }
    if (!found) {
        cout << "Employee not found.\n";
    }
}
void updateEmployee() {
    int id;
    cout << "Enter ID to update: ";
    cin >> id;
    int index = findEmployee(id);
    if (index == -1) {
        cout << "Employee not found.\n";
        return;
    }
    readDetails(emp[index]);
    saveFile();
    cout << "Employee updated.\n";
}
void deleteEmployee() {
    int id;
    cout << "Enter ID to delete: ";
    cin >> id;
    int index = findEmployee(id);
    if (index == -1) {
        cout << "Employee not found.\n";
        return;
    }
    for (int i = index; i < total - 1; i++) {
        emp[i] = emp[i + 1];
    }
    total--;
    saveFile();
    cout << "Employee deleted.\n";
}
void sortById() {
    for (int i = 0; i < total - 1; i++) {
        for (int j = 0; j < total - 1 - i; j++) {
            if (emp[j].id > emp[j + 1].id) {
                Employee temp = emp[j];
                emp[j] = emp[j + 1];
                emp[j + 1] = temp;
            }
        }
    }
    saveFile();
    cout << "Sorted by ID.\n";
}
int main() {
    int choice;
    loadFile();
    do {
        cout << "\n===== IMF Employee Management =====\n";
        cout << "1. Add employee\n";
        cout << "2. Display all\n";
        cout << "3. Search by ID\n";
        cout << "4. Search by name\n";
        cout << "5. Update employee\n";
        cout << "6. Delete employee\n";
        cout << "7. Sort by ID\n";
        cout << "8. Exit\n";
        cout << "Enter choice: ";
        cin >> choice;
        switch (choice) {
            case 1:
                addEmployee();
                break;
            case 2:
                displayAll();
                break;
            case 3:
                searchById();
                break;
            case 4:
                searchByName();
                break;
            case 5:
                updateEmployee();
                break;
            case 6:
                deleteEmployee();
                break;
            case 7:
                sortById();
                break;
            case 8:
                cout << "Goodbye!\n";
                break;
            default:
                cout << "Wrong choice. Try again.\n";
        }
    } while (choice != 8);
    return 0;
}
