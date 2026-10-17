#include <iostream>
#include <fstream>
#include <string>
using namespace std;

const int MAX = 10000;

class AttendanceManagement 
{
private:
    char attendance[MAX][32];
    int month, year, totalDays;
    string fileName;

    
    int undoID[10000], undoDay[10000];      
    int top = -1;

public:
    
    AttendanceManagement() 
    {
        cout << "Enter month (1-12): ";
        cin >> month;

        cout << "Enter year: ";
        cin >> year;

        cout << "Enter total days in this month: ";
        cin >> totalDays;

        fileName = "attendance.txt";

        for (int i = 0; i < MAX; i++)
        {
            for (int j = 0; j < 32; j++)
            {
                attendance[i][j] = '-';
            }
        }

        ifstream in(fileName);

        int id, day;
        char status;

        while (in >> id >> day >> status) {
            if (id >= 0 && id < MAX && day >= 1 && day <= 31 && (status == 'P' || status == 'A' || status == 'L')) 
            {
                attendance[id][day] = status;
            }
        }

        in.close();
    }

    
    void saveRecords() 
    {
        ofstream out(fileName);

        for (int i = 0; i < MAX; i++) 
        {
            for (int j = 1; j <= 31; j++) 
            {
                if (attendance[i][j] != '-') 
                {
                    out << i << " " << j << " "<< attendance[i][j] << "\n";
                }
            }
        }

        out.close();
    }

    
    int getID() 
    {
        int id;

        cout << "Enter Employee ID (last 4 digits): ";
        cin >> id;

        if (id < 0 || id >= MAX) 
        {
            cout << "Invalid Employee ID!\n";
            return -1;
        }

        return id;
    }

    
    void markAttendance() 
    {
        int id = getID();

        if (id == -1)
            return;

        int day;
        char status;

        cout << "Enter day (1-" << totalDays << "): ";
        cin >> day;

        if (day < 1 || day > totalDays) 
        {
            cout << "Invalid day!\n";
            return;
        }

        if (attendance[id][day] != '-') 
        {
            cout << "Attendance already marked!\n";
            return;
        }

        cout << "Enter status (P/A/L): ";
        cin >> status;

        if (status >= 'a' && status <= 'z')
            status = status - 32;

        if (status != 'P' && status != 'A' && status != 'L') 
        {
            cout << "Invalid status!\n";
            return;
        }

        if (top == 9999) 
        {
            cout << "Undo stack is full!\n";
            return;
        }

        attendance[id][day] = status;

        
        top++;
        undoID[top] = id;
        undoDay[top] = day;

        saveRecords();

        cout << "Attendance marked successfully!\n";
    }

    
    void displayRecords() 
    {
        bool found = false;

        cout << "\nEmployee ID\tDay\tStatus\n";

        for (int i = 0; i < MAX; i++) 
        {
            for (int j = 1; j <= totalDays; j++) 
            {
                if (attendance[i][j] != '-') 
                {
                    found = true;

                    cout << "EMP";

                    if (i < 1000) 
                    cout << "0";
                    if (i < 100) 
                    cout << "0";
                    if (i < 10) 
                    cout << "0";

                    cout << i << "\t\t"<< j << "\t"<< attendance[i][j] << "\n";
                }
            }
        }

        if (!found)
            cout << "No records found!\n";
    }

    
    void searchEmployee() 
    {
        int id = getID();

        if (id == -1)
            return;

        int present = 0, absent = 0, leave = 0;

        cout << "\nAttendance of EMP";

        if (id < 1000) 
        cout << "0";
        if (id < 100) 
        cout << "0";
        if (id < 10) 
        cout << "0";

        cout << id << ":\n";

        for (int day = 1; day <= totalDays; day++) 
        {
            if (attendance[id][day] != '-') 
            {
                cout << "Day " << day << ": "<< attendance[id][day] << "\n";

                if (attendance[id][day] == 'P')
                    present++;
                else if (attendance[id][day] == 'A')
                    absent++;
                else
                    leave++;
            }
        }

        int total = present + absent + leave;

        if (total == 0) 
        {
            cout << "No records found!\n";
        } 
        else 
        {
            cout << "Present: " << present << "\n";
            cout << "Absent: " << absent << "\n";
            cout << "Leave: " << leave << "\n";
            cout << "Attendance: " << present * 100 / total << "%\n";
        }
    }

    
    void searchEmployeeDay() 
    {
        int id = getID();

        if (id == -1)
            return;

        int day;

        cout << "Enter day (1-" << totalDays << "): ";
        cin >> day;

        if (day < 1 || day > totalDays) 
        {
            cout << "Invalid day!\n";
            return;
        }

        if (attendance[id][day] == '-')
            cout << "Attendance not marked!\n";
        else
            cout << "Attendance: " << attendance[id][day] << "\n";
    }

    
    void undoLast() 
    {
        if (top == -1) 
        {
            cout << "Nothing to undo!\n";
            return;
        }

        int id = undoID[top];
        int day = undoDay[top];

        top--;

        attendance[id][day] = '-';

        saveRecords();

        cout << "Last entry undone!\n";
    }
};

int main() 
{
    AttendanceManagement app;
    int choice;

    do 
    {
        cout << "\n===== ATTENDANCE MANAGEMENT =====\n";
        cout << "1. Mark Attendance\n";
        cout << "2. Display All Records\n";
        cout << "3. Search Employee\n";
        cout << "4. Search Employee on a Day\n";
        cout << "5. Undo Last Entry\n";
        cout << "6. Exit\n";

        cout << "Enter choice: ";
        cin >> choice;

        switch (choice) 
        {
            case 1:
                app.markAttendance();
                break;

            case 2:
                app.displayRecords();
                break;

            case 3:
                app.searchEmployee();
                break;

            case 4:
                app.searchEmployeeDay();
                break;

            case 5:
                app.undoLast();
                break;

            case 6:
                app.saveRecords();
                cout << "EXITED...\n";
                break;

            default:
                cout << "Invalid choice!\n";
        }
    } while (choice != 6);

    return 0;
}
