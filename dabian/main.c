#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#define DATA_DIR "data"
#define USERS_FILE DATA_DIR "/users.csv"
#define CATS_FILE  DATA_DIR "/categories.csv"
#define PRODS_FILE DATA_DIR "/products.csv"
#define ORDERS_FILE DATA_DIR "/orders.csv"
#define ITEMS_FILE  DATA_DIR "/order_items.csv"

#define MAX_USERS 1000
#define MAX_CATS  1000
#define MAX_PRODS 5000
#define MAX_ORDERS 5000
#define MAX_ITEMS 20000

#define STR_S 64
#define STR_M 128
#define STR_L 256

typedef struct {
    long long id;
    char username[STR_S];
    char password[STR_S];
    char role[STR_S]; // admin / staff
} User;

typedef struct {
    long long id;
    char name[STR_M];
} Category;

typedef struct {
    long long id;
    char name[STR_M];
    long long categoryId;
    double price;
    long long stock;
    char desc[STR_L];
} Product;

typedef struct {
    long long id;
    long long userId;
    char status[STR_S];   // pending/paid/shipped/done/canceled
    char createdAt[STR_S];
} Order;

typedef struct {
    long long id;
    long long orderId;
    long long productId;
    long long qty;
    double priceAtBuy;
} OrderItem;

/* ---------- Utils ---------- */
static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

static void safe_readline(const char *prompt, char *buf, size_t cap, int allowEmpty) {
    for (;;) {
        if (prompt) printf("%s", prompt);
        if (!fgets(buf, (int)cap, stdin)) {
            // EOF
            buf[0] = '\0';
            return;
        }
        trim_newline(buf);
        if (!allowEmpty && buf[0] == '\0') {
            printf("不能为空，请重试。\n");
            continue;
        }
        return;
    }
}

static long long read_ll(const char *prompt, long long minv, long long maxv) {
    char s[128];
    for (;;) {
        safe_readline(prompt, s, sizeof(s), 0);
        char *end = NULL;
        long long v = strtoll(s, &end, 10);
        if (end == s || *end != '\0') {
            printf("请输入整数。\n");
            continue;
        }
        if (v < minv || v > maxv) {
            printf("输入超出范围。\n");
            continue;
        }
        return v;
    }
}

static double read_double(const char *prompt, double minv, double maxv) {
    char s[128];
    for (;;) {
        safe_readline(prompt, s, sizeof(s), 0);
        char *end = NULL;
        double v = strtod(s, &end);
        if (end == s || *end != '\0') {
            printf("请输入数字。\n");
            continue;
        }
        if (v < minv || v > maxv) {
            printf("输入超出范围。\n");
            continue;
        }
        return v;
    }
}

static void now_iso(char out[STR_S]) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    strftime(out, STR_S, "%Y-%m-%d %H:%M:%S", lt);
}

static void ensure_data_dir(void) {
    // 如果目录已存在，MKDIR 可能失败，这里忽略错误
    MKDIR(DATA_DIR);
}

static void csv_sanitize(char *s) {
    // 替换逗号和换行，防止破坏CSV
    for (size_t i = 0; s && s[i]; ++i) {
        if (s[i] == ',') s[i] = ';';
        if (s[i] == '\n' || s[i] == '\r') s[i] = ' ';
    }
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

static long long next_id_ll(const long long *ids, int n) {
    long long mx = 0;
    for (int i = 0; i < n; ++i) if (ids[i] > mx) mx = ids[i];
    return mx + 1;
}

/* ---------- Load/Save ---------- */
static int load_users(User *arr, int cap) {
    int n = 0;
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) return 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (!line[0]) continue;
        User u; memset(&u, 0, sizeof(u));

        // id,username,password,role
        char *p = strtok(line, ",");
        if (!p) continue; u.id = atoll(p);
        p = strtok(NULL, ","); if (!p) continue; strncpy(u.username, p, STR_S-1);
        p = strtok(NULL, ","); if (!p) continue; strncpy(u.password, p, STR_S-1);
        p = strtok(NULL, ","); if (!p) continue; strncpy(u.role, p, STR_S-1);

        if (n < cap) arr[n++] = u;
    }
    fclose(f);
    return n;
}

static void save_users(const User *arr, int n) {
    FILE *f = fopen(USERS_FILE, "w");
    if (!f) { printf("写入失败：%s\n", USERS_FILE); return; }
    for (int i = 0; i < n; ++i) {
        User u = arr[i];
        csv_sanitize(u.username);
        csv_sanitize(u.password);
        csv_sanitize(u.role);
        fprintf(f, "%lld,%s,%s,%s\n", u.id, u.username, u.password, u.role);
    }
    fclose(f);
}

static int load_cats(Category *arr, int cap) {
    int n = 0;
    FILE *f = fopen(CATS_FILE, "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (!line[0]) continue;
        Category c; memset(&c, 0, sizeof(c));
        char *p = strtok(line, ",");
        if (!p) continue; c.id = atoll(p);
        p = strtok(NULL, ","); if (!p) continue; strncpy(c.name, p, STR_M-1);
        if (n < cap) arr[n++] = c;
    }
    fclose(f);
    return n;
}

static void save_cats(const Category *arr, int n) {
    FILE *f = fopen(CATS_FILE, "w");
    if (!f) { printf("写入失败：%s\n", CATS_FILE); return; }
    for (int i = 0; i < n; ++i) {
        Category c = arr[i];
        csv_sanitize(c.name);
        fprintf(f, "%lld,%s\n", c.id, c.name);
    }
    fclose(f);
}

static int load_prods(Product *arr, int cap) {
    int n = 0;
    FILE *f = fopen(PRODS_FILE, "r");
    if (!f) return 0;
    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (!line[0]) continue;
        Product p; memset(&p, 0, sizeof(p));
        // id,name,catId,price,stock,desc
        char *t = strtok(line, ","); if (!t) continue; p.id = atoll(t);
        t = strtok(NULL, ","); if (!t) continue; strncpy(p.name, t, STR_M-1);
        t = strtok(NULL, ","); if (!t) continue; p.categoryId = atoll(t);
        t = strtok(NULL, ","); if (!t) continue; p.price = atof(t);
        t = strtok(NULL, ","); if (!t) continue; p.stock = atoll(t);
        t = strtok(NULL, ","); if (!t) t = (char*)""; strncpy(p.desc, t, STR_L-1);

        if (n < cap) arr[n++] = p;
    }
    fclose(f);
    return n;
}

static void save_prods(const Product *arr, int n) {
    FILE *f = fopen(PRODS_FILE, "w");
    if (!f) { printf("写入失败：%s\n", PRODS_FILE); return; }
    for (int i = 0; i < n; ++i) {
        Product p = arr[i];
        csv_sanitize(p.name);
        csv_sanitize(p.desc);
        fprintf(f, "%lld,%s,%lld,%.2f,%lld,%s\n",
                p.id, p.name, p.categoryId, p.price, p.stock, p.desc);
    }
    fclose(f);
}

static int load_orders(Order *arr, int cap) {
    int n = 0;
    FILE *f = fopen(ORDERS_FILE, "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (!line[0]) continue;
        Order o; memset(&o, 0, sizeof(o));
        // id,userId,status,createdAt,0
        char *t = strtok(line, ","); if (!t) continue; o.id = atoll(t);
        t = strtok(NULL, ","); if (!t) continue; o.userId = atoll(t);
        t = strtok(NULL, ","); if (!t) continue; strncpy(o.status, t, STR_S-1);
        t = strtok(NULL, ","); if (!t) continue; strncpy(o.createdAt, t, STR_S-1);
        if (n < cap) arr[n++] = o;
    }
    fclose(f);
    return n;
}

static void save_orders(const Order *arr, int n) {
    FILE *f = fopen(ORDERS_FILE, "w");
    if (!f) { printf("写入失败：%s\n", ORDERS_FILE); return; }
    for (int i = 0; i < n; ++i) {
        Order o = arr[i];
        csv_sanitize(o.status);
        csv_sanitize(o.createdAt);
        fprintf(f, "%lld,%lld,%s,%s,0\n", o.id, o.userId, o.status, o.createdAt);
    }
    fclose(f);
}

static int load_items(OrderItem *arr, int cap) {
    int n = 0;
    FILE *f = fopen(ITEMS_FILE, "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (!line[0]) continue;
        OrderItem it; memset(&it, 0, sizeof(it));
        // id,orderId,productId,qty,priceAtBuy,0
        char *t = strtok(line, ","); if (!t) continue; it.id = atoll(t);
        t = strtok(NULL, ","); if (!t) continue; it.orderId = atoll(t);
        t = strtok(NULL, ","); if (!t) continue; it.productId = atoll(t);
        t = strtok(NULL, ","); if (!t) continue; it.qty = atoll(t);
        t = strtok(NULL, ","); if (!t) continue; it.priceAtBuy = atof(t);
        if (n < cap) arr[n++] = it;
    }
    fclose(f);
    return n;
}

static void save_items(const OrderItem *arr, int n) {
    FILE *f = fopen(ITEMS_FILE, "w");
    if (!f) { printf("写入失败：%s\n", ITEMS_FILE); return; }
    for (int i = 0; i < n; ++i) {
        OrderItem it = arr[i];
        fprintf(f, "%lld,%lld,%lld,%lld,%.2f,0\n",
                it.id, it.orderId, it.productId, it.qty, it.priceAtBuy);
    }
    fclose(f);
}

/* ---------- Lookup ---------- */
static const char* cat_name_by_id(const Category *cats, int n, long long id) {
    for (int i = 0; i < n; ++i) if (cats[i].id == id) return cats[i].name;
    return "(未知类目)";
}

static const char* user_name_by_id(const User *users, int n, long long id) {
    for (int i = 0; i < n; ++i) if (users[i].id == id) return users[i].username;
    return "(未知用户)";
}

static const char* prod_name_by_id(const Product *prods, int n, long long id) {
    for (int i = 0; i < n; ++i) if (prods[i].id == id) return prods[i].name;
    return "(未知商品)";
}

/* ---------- Init default ---------- */
static void init_if_empty(void) {
    ensure_data_dir();

    User users[MAX_USERS];
    int uN = load_users(users, MAX_USERS);
    if (uN == 0) {
        User admin = {1, "admin", "admin123", "admin"};
        users[uN++] = admin;
        save_users(users, uN);
    }

    Category cats[MAX_CATS];
    int cN = load_cats(cats, MAX_CATS);
    if (cN == 0) {
        Category a = {1, "生日蛋糕"};
        Category b = {2, "慕斯蛋糕"};
        Category c = {3, "面包甜点"};
        cats[cN++] = a; cats[cN++] = b; cats[cN++] = c;
        save_cats(cats, cN);
    }

    Product prods[MAX_PRODS];
    int pN = load_prods(prods, MAX_PRODS);
    if (pN == 0) {
        Product p1 = {1, "草莓奶油蛋糕", 1, 128.0, 50, "新鲜草莓+动物奶油"};
        Product p2 = {2, "巧克力慕斯",   2,  98.0, 40, "浓醇可可风味"};
        prods[pN++] = p1; prods[pN++] = p2;
        save_prods(prods, pN);
    }

    Order orders[MAX_ORDERS];
    OrderItem items[MAX_ITEMS];
    int oN = load_orders(orders, MAX_ORDERS);
    int iN = load_items(items, MAX_ITEMS);
    if (oN == 0) {
        char t[STR_S]; now_iso(t);
        Order o1; memset(&o1, 0, sizeof(o1));
        o1.id = 1; o1.userId = 1; strcpy(o1.status, "paid"); strncpy(o1.createdAt, t, STR_S-1);
        orders[oN++] = o1;

        OrderItem it1 = {1, 1, 1, 1, 128.0};
        OrderItem it2 = {2, 1, 2, 1,  98.0};
        items[iN++] = it1; items[iN++] = it2;

        save_orders(orders, oN);
        save_items(items, iN);
    }
}

/* ---------- Login ---------- */
static int login(User *outUser) {
    User users[MAX_USERS];
    int uN = load_users(users, MAX_USERS);

    printf("==============================\n");
    printf(" 网上蛋糕商城后台管理系统（C语言）\n");
    printf("==============================\n");
    printf("请输入管理员账号密码登录\n\n");

    for (int attempt = 1; attempt <= 3; ++attempt) {
        char uname[STR_S], pwd[STR_S];
        safe_readline("用户名：", uname, sizeof(uname), 0);
        safe_readline("密  码：", pwd, sizeof(pwd), 0);

        for (int i = 0; i < uN; ++i) {
            if (strcmp(users[i].username, uname) == 0 &&
                strcmp(users[i].password, pwd) == 0) {
                *outUser = users[i];
                printf("\n登录成功，欢迎你：%s（%s）\n", outUser->username, outUser->role);
                return 1;
            }
        }
        printf("用户名或密码错误（%d/3）\n\n", attempt);
    }
    printf("连续失败3次，程序退出。\n");
    return 0;
}

/* ---------- Category Menu ---------- */
static void category_menu(void) {
    for (;;) {
        Category cats[MAX_CATS];
        int cN = load_cats(cats, MAX_CATS);

        printf("\n========== 类目管理 ==========\n");
        printf("1. 类目列表\n");
        printf("2. 添加类目\n");
        printf("3. 修改类目\n");
        printf("4. 删除类目\n");
        printf("0. 返回上级\n");
        printf("请选择：");

        char op[16]; safe_readline(NULL, op, sizeof(op), 1);

        if (strcmp(op, "1") == 0) {
            printf("\n--- 类目列表 ---\n");
            if (cN == 0) printf("暂无类目。\n");
            for (int i = 0; i < cN; ++i) printf("%lld. %s\n", cats[i].id, cats[i].name);
        } else if (strcmp(op, "2") == 0) {
            Category c; memset(&c, 0, sizeof(c));
            long long ids[MAX_CATS];
            for (int i = 0; i < cN; ++i) ids[i] = cats[i].id;
            c.id = next_id_ll(ids, cN);
            safe_readline("类目名称：", c.name, sizeof(c.name), 0);
            cats[cN++] = c;
            save_cats(cats, cN);
            printf("添加成功，类目ID=%lld\n", c.id);
        } else if (strcmp(op, "3") == 0) {
            long long id = read_ll("输入要修改的类目ID：", 1, 999999999999LL);
            int idx = -1;
            for (int i = 0; i < cN; ++i) if (cats[i].id == id) { idx = i; break; }
            if (idx < 0) { printf("未找到。\n"); continue; }
            safe_readline("新名称：", cats[idx].name, sizeof(cats[idx].name), 0);
            save_cats(cats, cN);
            printf("修改完成。\n");
        } else if (strcmp(op, "4") == 0) {
            long long id = read_ll("输入要删除的类目ID：", 1, 999999999999LL);
            int idx = -1;
            for (int i = 0; i < cN; ++i) if (cats[i].id == id) { idx = i; break; }
            if (idx < 0) { printf("未找到。\n"); continue; }
            printf("确认删除【%s】？(y/n)：", cats[idx].name);
            char yn[8]; safe_readline(NULL, yn, sizeof(yn), 1);
            if (yn[0]=='y' || yn[0]=='Y') {
                for (int i = idx; i < cN-1; ++i) cats[i] = cats[i+1];
                cN--;
                save_cats(cats, cN);
                printf("删除成功。\n");
            } else printf("已取消。\n");
        } else if (strcmp(op, "0") == 0) {
            return;
        } else {
            printf("无效选项。\n");
        }
    }
}

/* ---------- Product Menu ---------- */
static void product_menu(void) {
    for (;;) {
        Product prods[MAX_PRODS];
        Category cats[MAX_CATS];
        int pN = load_prods(prods, MAX_PRODS);
        int cN = load_cats(cats, MAX_CATS);

        printf("\n========== 商品管理 ==========\n");
        printf("1. 商品列表\n");
        printf("2. 添加商品\n");
        printf("3. 修改商品\n");
        printf("4. 删除商品\n");
        printf("0. 返回上级\n");
        printf("请选择：");

        char op[16]; safe_readline(NULL, op, sizeof(op), 1);

        if (strcmp(op, "1") == 0) {
            printf("\n--- 商品列表 ---\n");
            if (pN == 0) { printf("暂无商品。\n"); continue; }
            printf("%-6s %-18s %-14s %-10s %-8s %s\n",
                   "ID", "名称", "类目", "价格", "库存", "描述");
            printf("----------------------------------------------------------------------\n");
            for (int i = 0; i < pN; ++i) {
                printf("%-6lld %-18.16s %-14.12s %-10.2f %-8lld %s\n",
                       prods[i].id,
                       prods[i].name,
                       cat_name_by_id(cats, cN, prods[i].categoryId),
                       prods[i].price,
                       prods[i].stock,
                       prods[i].desc);
            }
        } else if (strcmp(op, "2") == 0) {
            printf("\n--- 添加商品 ---\n");
            if (cN == 0) { printf("请先在【类目管理】中添加至少一个类目。\n"); continue; }

            Product p; memset(&p, 0, sizeof(p));
            long long ids[MAX_PRODS];
            for (int i = 0; i < pN; ++i) ids[i] = prods[i].id;
            p.id = next_id_ll(ids, pN);

            safe_readline("商品名称：", p.name, sizeof(p.name), 0);

            printf("可选类目：\n");
            for (int i = 0; i < cN; ++i) printf("  %lld. %s\n", cats[i].id, cats[i].name);

            p.categoryId = read_ll("类目ID：", 1, 999999999999LL);
            p.price = read_double("价格：", 0.0, 1e100);
            p.stock = read_ll("库存：", 0, 999999999999LL);
            safe_readline("描述：", p.desc, sizeof(p.desc), 1);

            prods[pN++] = p;
            save_prods(prods, pN);
            printf("添加成功，商品ID=%lld\n", p.id);
        } else if (strcmp(op, "3") == 0) {
            printf("\n--- 修改商品 ---\n");
            long long id = read_ll("输入要修改的商品ID：", 1, 999999999999LL);
            int idx = -1;
            for (int i = 0; i < pN; ++i) if (prods[i].id == id) { idx = i; break; }
            if (idx < 0) { printf("未找到该商品。\n"); continue; }

            char buf[256];

            printf("当前名称：%s\n", prods[idx].name);
            safe_readline("新名称（直接回车不改）：", buf, sizeof(buf), 1);
            if (buf[0]) strncpy(prods[idx].name, buf, STR_M-1);

            printf("当前类目：%s\n", cat_name_by_id(cats, cN, prods[idx].categoryId));
            safe_readline("新类目ID（直接回车不改）：", buf, sizeof(buf), 1);
            if (buf[0]) prods[idx].categoryId = atoll(buf);

            printf("当前价格：%.2f\n", prods[idx].price);
            safe_readline("新价格（直接回车不改）：", buf, sizeof(buf), 1);
            if (buf[0]) prods[idx].price = atof(buf);

            printf("当前库存：%lld\n", prods[idx].stock);
            safe_readline("新库存（直接回车不改）：", buf, sizeof(buf), 1);
            if (buf[0]) prods[idx].stock = atoll(buf);

            printf("当前描述：%s\n", prods[idx].desc);
            safe_readline("新描述（直接回车不改）：", buf, sizeof(buf), 1);
            if (buf[0]) strncpy(prods[idx].desc, buf, STR_L-1);

            save_prods(prods, pN);
            printf("修改完成。\n");
        } else if (strcmp(op, "4") == 0) {
            printf("\n--- 删除商品 ---\n");
            long long id = read_ll("输入要删除的商品ID：", 1, 999999999999LL);
            int idx = -1;
            for (int i = 0; i < pN; ++i) if (prods[i].id == id) { idx = i; break; }
            if (idx < 0) { printf("未找到该商品。\n"); continue; }

            printf("确认删除【%s】？(y/n)：", prods[idx].name);
            char yn[8]; safe_readline(NULL, yn, sizeof(yn), 1);
            if (yn[0]=='y' || yn[0]=='Y') {
                for (int i = idx; i < pN-1; ++i) prods[i] = prods[i+1];
                pN--;
                save_prods(prods, pN);
                printf("删除成功。\n");
            } else printf("已取消。\n");
        } else if (strcmp(op, "0") == 0) {
            return;
        } else {
            printf("无效选项。\n");
        }
    }
}

/* ---------- User Menu ---------- */
static void user_menu(void) {
    for (;;) {
        User users[MAX_USERS];
        int uN = load_users(users, MAX_USERS);

        printf("\n========== 用户管理 ==========\n");
        printf("1. 用户列表\n");
        printf("2. 新增用户\n");
        printf("3. 删除用户\n");
        printf("4. 重置密码\n");
        printf("0. 返回上级\n");
        printf("请选择：");

        char op[16]; safe_readline(NULL, op, sizeof(op), 1);

        if (strcmp(op, "1") == 0) {
            printf("\n--- 用户列表 ---\n");
            printf("%-6s %-16s %s\n", "ID", "用户名", "角色");
            printf("--------------------------------\n");
            for (int i = 0; i < uN; ++i) {
                printf("%-6lld %-16s %s\n", users[i].id, users[i].username, users[i].role);
            }
        } else if (strcmp(op, "2") == 0) {
            printf("\n--- 新增用户 ---\n");
            User u; memset(&u, 0, sizeof(u));

            char uname[STR_S];
            safe_readline("用户名：", uname, sizeof(uname), 0);
            int exists = 0;
            for (int i = 0; i < uN; ++i) if (strcmp(users[i].username, uname) == 0) exists = 1;
            if (exists) { printf("用户名已存在。\n"); continue; }

            strncpy(u.username, uname, STR_S-1);
            safe_readline("密码：", u.password, sizeof(u.password), 0);
            safe_readline("角色(admin/staff)：", u.role, sizeof(u.role), 0);
            if (strcmp(u.role, "admin") != 0 && strcmp(u.role, "staff") != 0) strcpy(u.role, "staff");

            long long ids[MAX_USERS];
            for (int i = 0; i < uN; ++i) ids[i] = users[i].id;
            u.id = next_id_ll(ids, uN);

            users[uN++] = u;
            save_users(users, uN);
            printf("创建成功，用户ID=%lld\n", u.id);
        } else if (strcmp(op, "3") == 0) {
            long long id = read_ll("输入要删除的用户ID：", 1, 999999999999LL);
            int idx = -1;
            for (int i = 0; i < uN; ++i) if (users[i].id == id) { idx = i; break; }
            if (idx < 0) { printf("未找到。\n"); continue; }
            if (strcmp(users[idx].username, "admin") == 0) { printf("不允许删除默认管理员。\n"); continue; }

            printf("确认删除用户【%s】？(y/n)：", users[idx].username);
            char yn[8]; safe_readline(NULL, yn, sizeof(yn), 1);
            if (yn[0]=='y' || yn[0]=='Y') {
                for (int i = idx; i < uN-1; ++i) users[i] = users[i+1];
                uN--;
                save_users(users, uN);
                printf("删除成功。\n");
            } else printf("已取消。\n");
        } else if (strcmp(op, "4") == 0) {
            long long id = read_ll("输入要重置密码的用户ID：", 1, 999999999999LL);
            int idx = -1;
            for (int i = 0; i < uN; ++i) if (users[i].id == id) { idx = i; break; }
            if (idx < 0) { printf("未找到。\n"); continue; }
            safe_readline("新密码：", users[idx].password, sizeof(users[idx].password), 0);
            save_users(users, uN);
            printf("密码已重置。\n");
        } else if (strcmp(op, "0") == 0) {
            return;
        } else {
            printf("无效选项。\n");
        }
    }
}

/* ---------- Order Menu ---------- */
static int status_ok(const char *s) {
    return strcmp(s,"pending")==0 || strcmp(s,"paid")==0 || strcmp(s,"shipped")==0
           || strcmp(s,"done")==0 || strcmp(s,"canceled")==0;
}

static void order_menu(void) {
    for (;;) {
        Order orders[MAX_ORDERS];
        OrderItem items[MAX_ITEMS];
        User users[MAX_USERS];
        Product prods[MAX_PRODS];

        int oN = load_orders(orders, MAX_ORDERS);
        int iN = load_items(items, MAX_ITEMS);
        int uN = load_users(users, MAX_USERS);
        int pN = load_prods(prods, MAX_PRODS);

        printf("\n========== 订单管理 ==========\n");
        printf("1. 订单列表\n");
        printf("2. 查看订单明细\n");
        printf("3. 修改订单状态\n");
        printf("0. 返回上级\n");
        printf("请选择：");

        char op[16]; safe_readline(NULL, op, sizeof(op), 1);

        if (strcmp(op, "1") == 0) {
            printf("\n--- 订单列表 ---\n");
            if (oN == 0) { printf("暂无订单。\n"); continue; }
            printf("%-8s %-16s %-12s %s\n", "订单ID", "用户", "状态", "创建时间");
            printf("------------------------------------------------------------\n");
            for (int i = 0; i < oN; ++i) {
                printf("%-8lld %-16.14s %-12s %s\n",
                       orders[i].id,
                       user_name_by_id(users, uN, orders[i].userId),
                       orders[i].status,
                       orders[i].createdAt);
            }
        } else if (strcmp(op, "2") == 0) {
            long long oid = read_ll("输入订单ID：", 1, 999999999999LL);
            int idx = -1;
            for (int i = 0; i < oN; ++i) if (orders[i].id == oid) { idx = i; break; }
            if (idx < 0) { printf("未找到。\n"); continue; }

            printf("\n订单ID: %lld\n用户: %s\n状态: %s\n时间: %s\n",
                   orders[idx].id,
                   user_name_by_id(users, uN, orders[idx].userId),
                   orders[idx].status,
                   orders[idx].createdAt);

            printf("\n明细：\n");
            printf("%-6s %-18s %-8s %s\n", "ID", "商品", "数量", "购买价");
            printf("-----------------------------------------------\n");
            double total = 0.0;
            for (int i = 0; i < iN; ++i) {
                if (items[i].orderId == oid) {
                    printf("%-6lld %-18.16s %-8lld %.2f\n",
                           items[i].id,
                           prod_name_by_id(prods, pN, items[i].productId),
                           items[i].qty,
                           items[i].priceAtBuy);
                    total += items[i].qty * items[i].priceAtBuy;
                }
            }
            printf("合计: %.2f\n", total);
        } else if (strcmp(op, "3") == 0) {
            long long oid = read_ll("输入订单ID：", 1, 999999999999LL);
            int idx = -1;
            for (int i = 0; i < oN; ++i) if (orders[i].id == oid) { idx = i; break; }
            if (idx < 0) { printf("未找到。\n"); continue; }

            printf("当前状态：%s\n", orders[idx].status);
            printf("可选状态：pending / paid / shipped / done / canceled\n");
            char st[STR_S];
            safe_readline("新状态：", st, sizeof(st), 0);
            if (!status_ok(st)) { printf("状态不合法。\n"); continue; }
            strncpy(orders[idx].status, st, STR_S-1);
            save_orders(orders, oN);
            printf("修改完成。\n");
        } else if (strcmp(op, "0") == 0) {
            return;
        } else {
            printf("无效选项。\n");
        }
    }
}

/* ---------- Main ---------- */
int main(void) {
    init_if_empty();

    User cur; memset(&cur, 0, sizeof(cur));
    if (!login(&cur)) return 0;

    if (strcmp(cur.role, "admin") != 0) {
        printf("当前账号不是管理员，无法进入后台。\n");
        return 0;
    }

    for (;;) {
        printf("\n==============================\n");
        printf(" 主界面（后台管理）\n");
        printf(" 当前用户: %s\n", cur.username);
        printf("==============================\n");
        printf("1. 商品管理\n");
        printf("2. 类目管理\n");
        printf("3. 用户管理\n");
        printf("4. 订单管理\n");
        printf("0. 退出系统\n");
        printf("请选择：");

        char op[16];
        safe_readline(NULL, op, sizeof(op), 1);

        if (strcmp(op, "1") == 0) product_menu();
        else if (strcmp(op, "2") == 0) category_menu();
        else if (strcmp(op, "3") == 0) user_menu();
        else if (strcmp(op, "4") == 0) order_menu();
        else if (strcmp(op, "0") == 0) { printf("已退出。\n"); break; }
        else printf("无效选项。\n");
    }

    return 0;
}
