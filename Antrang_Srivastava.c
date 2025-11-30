#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define NM 64
#define DP 32
#define LN 256
typedef struct {
    int id;
    char dept[DP];
    int cap;
    int occ;
} Unit;
typedef struct {
    int id;
    char nm[NM];
    int age;
    int sev;
    int unit_id;
} Person;
static Unit *units = NULL;
static int ucnt = 0;
static Person **active = NULL;
static int acnt = 0;
static Person **queue = NULL;
static int qcnt = 0;
static void *must_malloc(size_t n) {
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "malloc %zu failed: %s\n", n, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}
static void *must_realloc(void *ptr, size_t n) {
    void *p = realloc(ptr, n);
    if (!p && n != 0) {
        fprintf(stderr, "realloc %zu failed: %s\n", n, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}
static void read_line(char *buf, size_t len) {
    if (!fgets(buf, (int)len, stdin)) { buf[0] = '\0'; return; }
    size_t L = strlen(buf);
    if (L && buf[L - 1] == '\n') buf[L - 1] = '\0';
}
static int read_int_prompt(const char *prompt, int *out) {
    char s[LN];
    if (prompt) { printf("%s", prompt); fflush(stdout); }
    read_line(s, sizeof(s));
    if (!s[0]) return 0;
    char *e = NULL;
    long v = strtol(s, &e, 10);
    if (e == s || *e != '\0') return 0;
    *out = (int)v;
    return 1;
}
static Unit *get_unit_by_id(int id) {
    for (int i = 0; i < ucnt; i++) if (units[i].id == id) return &units[i];
    return NULL;
}
static void add_unit(int id, const char *dept, int cap) {
    units = must_realloc(units, sizeof(Unit) * (ucnt + 1));
    units[ucnt].id = id;
    strncpy(units[ucnt].dept, dept, DP - 1);
    units[ucnt].dept[DP - 1] = '\0';
    units[ucnt].cap = cap;
    units[ucnt].occ = 0;
    ucnt++;
}
static void show_units(void) {
    if (ucnt == 0) { printf("No wards defined yet.\n"); return; }
    printf("Wards:\n");
    printf("ID\tDept\t\tCapacity\tOccupied\tVacant\n");
    for (int i = 0; i < ucnt; i++) {
        printf("%d\t%-10s\t%d\t\t%d\t\t%d\n",
               units[i].id, units[i].dept, units[i].cap, units[i].occ,
               units[i].cap - units[i].occ);
    }
}
static int assign_person(Person *p) {
    if (!p) return 0;
    int pick = -1, best_vac = 0;
    for (int i = 0; i < ucnt; i++) {
        int vac = units[i].cap - units[i].occ;
        if (vac > 0 && (pick == -1 || vac > best_vac)) { pick = i; best_vac = vac; }
    }
    if (pick == -1) return 0;
    units[pick].occ++;
    p->unit_id = units[pick].id;
    active = must_realloc(active, sizeof(Person*) * (acnt + 1));
    active[acnt++] = p;
    return 1;
}
static void add_active(Person *p) {
    active = must_realloc(active, sizeof(Person*) * (acnt + 1));
    active[acnt++] = p;
}
static Person *new_person(const char *name, int age, int sev) {
    static int next_pid = 1000;
    Person *p = must_malloc(sizeof(Person));
    p->id = next_pid++;
    strncpy(p->nm, name, NM - 1);
    p->nm[NM - 1] = '\0';
    p->age = age;
    if (sev < 1) sev = 1;
    if (sev > 10) sev = 10;
    p->sev = sev;
    p->unit_id = -1;
    return p;
}
static void queue_insert_desc(Person *p) {
    int pos = qcnt;
    for (int i = 0; i < qcnt; i++) { if (p->sev > queue[i]->sev) { pos = i; break; } }
    queue = must_realloc(queue, sizeof(Person*) * (qcnt + 1));
    for (int j = qcnt; j > pos; j--) queue[j] = queue[j - 1];
    queue[pos] = p;
    qcnt++;
}
static void queue_remove_at(int idx) {
    if (idx < 0 || idx >= qcnt) return;
    for (int i = idx; i < qcnt - 1; i++) queue[i] = queue[i + 1];
    qcnt--;
    if (qcnt == 0) { free(queue); queue = NULL; }
    else { queue = must_realloc(queue, sizeof(Person*) * qcnt); }
}
static void try_assign_queue(void) {
    if (qcnt == 0) return;
    int i = 0;
    while (i < qcnt) {
        Person *p = queue[i];
        if (assign_person(p)) {
            printf("Allocated waiting patient %s (ID %d) to ward %d.\n", p->nm, p->id, p->unit_id);
            queue_remove_at(i);
        } else {
            i++;
        }
    }
}
static void admit_interactive(void) {
    char name[NM];
    int age = 0, sev = 0;
    printf("Enter patient name: ");
    read_line(name, sizeof(name));
    if (!name[0]) { printf("Name cannot be empty.\n"); return; }
    if (!read_int_prompt("Enter age: ", &age)) { printf("Invalid age.\n"); return; }
    do {
        printf("Enter severity (1-10; 10 most severe): ");
        char s[LN]; read_line(s, sizeof(s));
        char *e = NULL; long v = strtol(s, &e, 10);
        if (e == s || *e != '\0') { printf("Invalid severity.\n"); continue; }
        sev = (int)v;
        if (sev < 1 || sev > 10) { printf("Severity must be between 1 and 10.\n"); sev = 0; }
    } while (sev < 1 || sev > 10);
    Person *p = new_person(name, age, sev);
    add_active(p);
    if (assign_person(p)) {
        printf("Patient %s (ID %d) admitted to ward %d.\n", p->nm, p->id, p->unit_id);
    } else {
        queue_insert_desc(p);
        printf("No beds available. Patient %s (ID %d) added to waiting list (severity %d).\n", p->nm, p->id, p->sev);
    }
}
static void discharge_interactive(void) {
    if (acnt == 0 && qcnt == 0) { printf("No patients in system.\n"); return; }
    int id;
    if (!read_int_prompt("Enter patient ID to discharge: ", &id)) { printf("Invalid ID.\n"); return; }
    int idx = -1;
    for (int i = 0; i < acnt; i++) if (active[i]->id == id) { idx = i; break; }
    if (idx == -1) {
        int widx = -1;
        for (int j = 0; j < qcnt; j++) if (queue[j]->id == id) { widx = j; break; }
        if (widx == -1) { printf("Patient ID %d not found.\n", id); return; }
        Person *p = queue[widx];
        queue_remove_at(widx);
        free(p);
        printf("Patient removed from waiting list.\n");
        return;
    }
    Person *p = active[idx];
    if (p->unit_id != -1) {
        Unit *u = get_unit_by_id(p->unit_id);
        if (u && u->occ > 0) u->occ--;
        printf("Patient %s (ID %d) discharged from ward %d.\n", p->nm, p->id, p->unit_id);
    } else {
        int widx = -1;
        for (int j = 0; j < qcnt; j++) if (queue[j] == p) { widx = j; break; }
        if (widx != -1) {
            queue_remove_at(widx);
            printf("Patient %s (ID %d) removed from waiting list.\n", p->nm, p->id);
        } else {
            printf("Patient %s (ID %d) was not admitted nor in waiting list.\n", p->nm, p->id);
        }
    }

    free(p);
    for (int j = idx; j < acnt - 1; j++) active[j] = active[j + 1];
    acnt--;
    if (acnt == 0) { free(active); active = NULL; }
    else { active = must_realloc(active, sizeof(Person*) * acnt); }
    try_assign_queue();
}
static void show_people(void) {
    if (acnt == 0 && qcnt == 0) { printf("No patients in system.\n"); return; }
    if (acnt > 0) {
        printf("Patients:\n");
        printf("ID\tName\t\tAge\tSeverity\tWardAssigned\n");
        for (int i = 0; i < acnt; i++) {
            Person *p = active[i];
            printf("%d\t%-12s\t%d\t%d\t\t", p->id, p->nm, p->age, p->sev);
            if (p->unit_id == -1) printf("Waiting\n"); else printf("%d\n", p->unit_id);
        }
    }
    if (qcnt > 0) {
        printf("\nWaiting list (by severity desc):\n");
        for (int i = 0; i < qcnt; i++) {
            printf("%d) ID %d, Name: %s, Severity: %d\n", i + 1, queue[i]->id, queue[i]->nm, queue[i]->sev);
        }
    }
}
static void show_report(void) {
    if (ucnt == 0) { printf("No wards defined.\n"); return; }
    int tcap = 0, tocc = 0;
    for (int i = 0; i < ucnt; i++) { tcap += units[i].cap; tocc += units[i].occ; }
    printf("Total capacity: %d, Total occupied: %d, Total vacant: %d\n", tcap, tocc, tcap - tocc);
    show_units();
    printf("Patients waiting: %d\n", qcnt);
}
static void release_all(void) {
    if (active) {
        for (int i = 0; i < acnt; i++) free(active[i]);
        free(active);
        active = NULL;
    }
    if (queue) { free(queue); queue = NULL; }
    if (units) { free(units); units = NULL; }
    ucnt = acnt = qcnt = 0;
}
int main(void) {
    add_unit(1, "General", 5);
    add_unit(2, "ICU", 2);
    add_unit(3, "Pediatrics", 3);
    for (;;) {
        printf("\n--- Hospital Bed & Resource Allocation ---\n");
        printf("1) List wards\n");
        printf("2) Add ward\n");
        printf("3) Admit patient\n");
        printf("4) Discharge patient\n");
        printf("5) List patients & waiting list\n");
        printf("6) Reports\n");
        printf("7) Exit\n");
        printf("Choose option: ");
        char line[LN]; read_line(line, sizeof(line));
        if (!line[0]) continue;
        int ch = atoi(line);

        switch (ch) {
            case 1: show_units(); break;
            case 2: {
                int wid = 0, cap = 0;
                char dept[DP];
                if (!read_int_prompt("Enter new ward id: ", &wid)) { printf("Invalid ward id.\n"); break; }
                printf("Enter department name (no spaces): ");
                read_line(dept, sizeof(dept));
                if (!dept[0]) { printf("Invalid department.\n"); break; }
                if (!read_int_prompt("Enter capacity: ", &cap)) { printf("Invalid capacity.\n"); break; }
                if (cap <= 0) { printf("Capacity must be positive.\n"); break; }
                add_unit(wid, dept, cap);
                printf("Ward added.\n");
                break;
            }
            case 3: admit_interactive(); break;
            case 4: discharge_interactive(); break;
            case 5: show_people(); break;
            case 6: show_report(); break;
            case 7: release_all(); printf("Exiting.\n"); return 0;
            default: printf("Invalid choice.\n"); break;
        }
    }

    release_all();
    return 0;
}