#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NAME_LEN 64
#define DEPT_LEN 32
#define LINE_LEN 256

typedef struct {
    int ward_id;
    char department[DEPT_LEN];
    int capacity;
    int occupied;
} Ward;

typedef struct {
    int patient_id;
    char name[NAME_LEN];
    int age;
    int severity;
    int ward_assigned;
} Patient;

static Ward *wards = NULL;
static int ward_count = 0;

static Patient **patients = NULL;
static int patient_count = 0;

static Patient **waiting_list = NULL;
static int waiting_count = 0;

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "malloc failed (%zu bytes): %s\n", n, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}

static void *xrealloc(void *ptr, size_t n) {
    void *p = realloc(ptr, n);
    if (!p && n != 0) {
        fprintf(stderr, "realloc failed (%zu bytes): %s\n", n, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}

static void get_line(char *buf, size_t len) {
    if (!fgets(buf, (int)len, stdin)) {
        buf[0] = '\0';
        return;
    }
    size_t l = strlen(buf);
    if (l > 0 && buf[l-1] == '\n') buf[l-1] = '\0';
}

static int read_int(const char *prompt, int *out) {
    char line[LINE_LEN];
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    get_line(line, sizeof(line));
    if (line[0] == '\0') 
    return 0;
    char *endptr;
    long val = strtol(line, &endptr, 10);
    if (endptr == line || *endptr != '\0') 
    return 0;
    *out = (int)val;
    return 1;
}

void add_ward(int ward_id, const char *dept, int capacity) {
    wards = xrealloc(wards, sizeof(Ward) * (ward_count + 1));
    wards[ward_count].ward_id = ward_id;
    strncpy(wards[ward_count].department, dept, DEPT_LEN - 1);
    wards[ward_count].department[DEPT_LEN - 1] = '\0';
    wards[ward_count].capacity = capacity;
    wards[ward_count].occupied = 0;
    ward_count++;
}

void list_wards() {
    if (ward_count == 0) {
        printf("No wards defined yet.\n");
        return;
    }
    printf("Wards:\n");
    printf("ID\tDept\t\tCapacity\tOccupied\tVacant\n");
    for (int i = 0; i < ward_count; ++i) {
        printf("%d\t%-10s\t%d\t\t%d\t\t%d\n",
               wards[i].ward_id,
               wards[i].department,
               wards[i].capacity,
               wards[i].occupied,
               wards[i].capacity - wards[i].occupied);
    }
}

Ward* find_ward_by_id(int ward_id) {
    for (int i = 0; i < ward_count; ++i) {
        if (wards[i].ward_id == ward_id)
         return &wards[i];
    }
    return NULL;
}

int allocate_bed_to_patient(Patient *p) {
    if (!p) 
    return 0;
    int best_index = -1;
    int best_vacant = 0;
    for (int i = 0; i < ward_count; ++i) {
        int vacant = wards[i].capacity - wards[i].occupied;
        if (vacant > 0) {
            if (best_index == -1 || vacant > best_vacant) {
                best_index = i;
                best_vacant = vacant;
            }
        }
    }
    if (best_index == -1) 
    return 0;
    wards[best_index].occupied++;
    p->ward_assigned = wards[best_index].ward_id;
    return 1;
}

void insert_patient_record(Patient *p) {
    patients = xrealloc(patients, sizeof(Patient*) * (patient_count + 1));
    patients[patient_count++] = p;
}

Patient* create_patient(const char *name, int age, int severity) {
    Patient *p = xmalloc(sizeof(Patient));
    static int next_id = 1000;
    p->patient_id = next_id++;
    strncpy(p->name, name, NAME_LEN - 1);
    p->name[NAME_LEN - 1] = '\0';
    p->age = age;
    p->severity = severity;
    p->ward_assigned = -1;
    return p;
}

void add_patient_to_waitlist_sorted(Patient *p) {
    int pos = waiting_count;
    for (int i = 0; i < waiting_count; ++i) {
        if (p->severity > waiting_list[i]->severity) { pos = i; break; }
    }
    waiting_list = xrealloc(waiting_list, sizeof(Patient*) * (waiting_count + 1));
    for (int j = waiting_count; j > pos; --j) waiting_list[j] = waiting_list[j-1];
    waiting_list[pos] = p;
    waiting_count++;
}

void remove_from_waitlist_by_index(int idx) {
    if (idx < 0 || idx >= waiting_count) return;
    for (int i = idx; i < waiting_count - 1; ++i) waiting_list[i] = waiting_list[i+1];
    waiting_count--;
    if (waiting_count == 0) {
        free(waiting_list);
        waiting_list = NULL;
    } else {
        waiting_list = xrealloc(waiting_list, sizeof(Patient*) * waiting_count);
    }
}

void try_allocate_waiting() {
    if (waiting_count == 0)
    return;
    int i = 0;
    while (i < waiting_count) {
        Patient *p = waiting_list[i];
        if (allocate_bed_to_patient(p)) {
            printf("Allocated waiting patient %s (ID %d) to ward %d.\n",
                   p->name, p->patient_id, p->ward_assigned);
            remove_from_waitlist_by_index(i);
        } else {
            ++i;
        }
    }
}

void admit_patient_interactive() {
    char name[NAME_LEN];
    char line[LINE_LEN];
    int age = 0, severity = 0;

    printf("Enter patient name: ");
    get_line(name, sizeof(name));
    if (name[0] == '\0') { printf("Name cannot be empty.\n"); 
        return; 
    }

    if (!read_int("Enter age: ", &age)) { printf("Invalid age.\n"); 
        return; 
    }
    if (!read_int("Enter severity (1-10; 10 most severe): ", &severity)) { printf("Invalid severity.\n");
        return; 
    }
    if (severity < 1) severity = 1;
    if (severity > 10) severity = 10;

    Patient *p = create_patient(name, age, severity);
    insert_patient_record(p);

    if (allocate_bed_to_patient(p)) {
        printf("Patient %s (ID %d) admitted to ward %d.\n", p->name, p->patient_id, p->ward_assigned);
    } else {
        add_patient_to_waitlist_sorted(p);
        printf("No beds available. Patient %s (ID %d) added to waiting list (severity %d).\n",
               p->name, p->patient_id, p->severity);
    }
}

void discharge_patient_interactive() {
    if (patient_count == 0) { printf("No patients in system.\n"); 
        return; 
    }
    int id;
    if (!read_int("Enter patient ID to discharge: ", &id)) { printf("Invalid ID.\n"); 
        return; 
    }

    int idx = -1;
    for (int i = 0; i < patient_count; ++i) {
        if (patients[i]->patient_id == id) { idx = i; 
            break;
        }
    }
    if (idx == -1) { printf("Patient ID %d not found.\n", id);
        return; 
    }

    Patient *p = patients[idx];

    if (p->ward_assigned != -1) {
        Ward *w = find_ward_by_id(p->ward_assigned);
        if (w && w->occupied > 0) w->occupied--;
        printf("Patient %s (ID %d) discharged from ward %d.\n", p->name, p->patient_id, p->ward_assigned);
    } else {
        int widx = -1;
        for (int j = 0; j < waiting_count; ++j) {
            if (waiting_list[j] == p) { widx = j; 
                break; 
            }
        }
        if (widx != -1) {
            remove_from_waitlist_by_index(widx);
            printf("Patient %s (ID %d) removed from waiting list.\n", p->name, p->patient_id);
        } else {
            printf("Patient %s (ID %d) was not admitted nor in waiting list.\n", p->name, p->patient_id);
        }
    }

    free(p);
    for (int j = idx; j < patient_count - 1; ++j) patients[j] = patients[j+1];
    patient_count--;
    if (patient_count == 0) {
        free(patients);
        patients = NULL;
    } else {
        patients = xrealloc(patients, sizeof(Patient*) * patient_count);
    }

    try_allocate_waiting();
}

void list_patients() {
    if (patient_count == 0) { 
    printf("No patients in system.\n");
    return; 
}
    printf("Patients:\n");
    printf("ID\tName\t\tAge\tSeverity\tWardAssigned\n");
    for (int i = 0; i < patient_count; ++i) {
        Patient *p = patients[i];
        printf("%d\t%-12s\t%d\t%d\t\t", p->patient_id, p->name, p->age, p->severity);
        if (p->ward_assigned == -1) printf("Waiting\n"); 
        else
        printf("%d\n", p->ward_assigned);
    }
    if (waiting_count > 0) {
        printf("\nWaiting list (by severity desc):\n");
        for (int i = 0; i < waiting_count; ++i) {
            printf("%d) ID %d, Name: %s, Severity: %d\n",
                   i + 1, waiting_list[i]->patient_id, waiting_list[i]->name, waiting_list[i]->severity);
        }
    }
}

void reports() {
    if (ward_count == 0) { 
        printf("No wards defined.\n");
        return;
    }
    int total_capacity = 0, total_occupied = 0;
    for (int i = 0; i < ward_count; ++i) {
        total_capacity += wards[i].capacity;
        total_occupied += wards[i].occupied;
    }
    printf("Total capacity: %d, Total occupied: %d, Total vacant: %d\n",
           total_capacity, total_occupied, total_capacity - total_occupied);
    list_wards();
    printf("Patients waiting: %d\n", waiting_count);
}

void free_all() {
    if (patients) {
        for (int i = 0; i < patient_count; ++i) 
        free(patients[i]);
        free(patients);
        patients = NULL;
    }
    if (waiting_list) {
        free(waiting_list); waiting_list = NULL;
     }
    if (wards) {
        free(wards); wards = NULL;
     }
    ward_count = patient_count = waiting_count = 0;
}

int main(void) {
    add_ward(1, "General", 5);
    add_ward(2, "ICU", 2);
    add_ward(3, "Pediatrics", 3);

    while (1) {
        printf("\n--- Hospital Bed & Resource Allocation ---\n");
        printf("1) List wards\n");
        printf("2) Add ward\n");
        printf("3) Admit patient\n");
        printf("4) Discharge patient\n");
        printf("5) List patients & waiting list\n");
        printf("6) Reports\n");
        printf("7) Exit\n");
        printf("Choose option: ");
        char choice_line[LINE_LEN];
        get_line(choice_line, sizeof(choice_line));
        if (choice_line[0] == '\0')
        continue;
        int choice = atoi(choice_line);

        switch (choice) {
            case 1: list_wards(); 
            break;
            case 2: {
                int wid = 0, cap = 0;
                char dept[DEPT_LEN];
                if (!read_int("Enter new ward id: ", &wid)) { printf("Invalid ward id.\n");
                    break;
                }
                printf("Enter department name (no spaces): ");
                get_line(dept, sizeof(dept));
                if (dept[0] == '\0') { printf("Invalid department.\n");
                    break;
                }
                if (!read_int("Enter capacity: ", &cap)) { printf("Invalid capacity.\n");
                    break; 
                }
                if (cap <= 0) { printf("Capacity must be positive.\n");
                    break; 
                }
                add_ward(wid, dept, cap);
                printf("Ward added.\n");
                break;
            }
            case 3: admit_patient_interactive();
            break;
            case 4: discharge_patient_interactive();
            break;
            case 5: list_patients();
            break;
            case 6: reports();
            break;
            case 7: free_all(); printf("Exiting.\n");
            return 0;
            default:
         printf("Invalid choice.\n");
        }
    }

    free_all();
    return 0;
}
