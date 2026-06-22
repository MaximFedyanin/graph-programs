#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

/* Ceiling-деление для неотрицательных a и положительного b */
long long ceil_div(long long a, long long b) {
    return (a + b - 1) / b;
}

int main(void) {
    setlocale(LC_ALL, "ru_RU.UTF-8");

    int n_start, n_end, p_start, p_end;

    /* Приглашения на ввод */
    printf("n_start = ");
    if (scanf("%d", &n_start) != 1) {
        fprintf(stderr, "Ошибка ввода n_start\n");
        return 1;
    }
    
    printf("n_finish = ");
    if (scanf("%d", &n_end) != 1) {
        fprintf(stderr, "Ошибка ввода n_finish\n");
        return 1;
    }
    
    printf("p_start = ");
    if (scanf("%d", &p_start) != 1) {
        fprintf(stderr, "Ошибка ввода p_start\n");
        return 1;
    }
    
    printf("p_finish = ");
    if (scanf("%d", &p_end) != 1) {
        fprintf(stderr, "Ошибка ввода p_finish\n");
        return 1;
    }

    FILE *f = fopen("dobro.xls", "w");
    if (!f) {
        fprintf(stderr, "Не удалось открыть dobro.xls для записи.\n");
        return 1;
    }

    /* --- Заголовок HTML-документа, распознаваемого Excel --- */
    fprintf(f, "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"\n");
    fprintf(f, "      xmlns:x=\"urn:schemas-microsoft-com:office:excel\"\n");
    fprintf(f, "      xmlns=\"http://www.w3.org/TR/REC-html40\">\n");
    fprintf(f, "<head><meta http-equiv=\"Content-Type\" "
               "content=\"text/html; charset=utf-8\"></head>\n");
    fprintf(f, "<body>\n");

    /* === Строка пояснения с формулами === */
    fprintf(f, "<table border=\"0\" cellspacing=\"0\" cellpadding=\"3\">\n");
    fprintf(f, "<tr><td colspan=\"100\">\n");
    fprintf(f, "<b>первое число</b> = ⌈(n-1)/p⌉ + 1 &nbsp;&nbsp;&nbsp; ");
    fprintf(f, "<b>второе число</b> = ⌈n/(p-1)⌉ &nbsp;&nbsp;&nbsp; ");
    fprintf(f, "<b>третье число</b> = min{k : ⌈n/k⌉·C(k,2) + C(n mod k, 2) ≥ ⌈C(n,2)/p⌉}\n");
    fprintf(f, "</td></tr>\n");
    fprintf(f, "</table>\n<br>\n");

    /* === Основная таблица === */
    fprintf(f, "<table border=\"1\" cellspacing=\"0\" cellpadding=\"3\">\n");

    /* === Строка 1: пустая ячейка, "n", затем значения n === */
    fprintf(f, "<tr>\n<td></td>\n<td>n</td>\n");
    for (int n = n_start; n <= n_end; n++) {
        fprintf(f, "<td>%d</td>\n", n);
    }
    fprintf(f, "</tr>\n");

    /* === Строка 2: "p", затем пустые ячейки === */
    fprintf(f, "<tr>\n<td>p</td>\n<td></td>\n");
    for (int n = n_start; n <= n_end; n++) {
        fprintf(f, "<td></td>\n");
    }
    fprintf(f, "</tr>\n");

    /* === Строки данных: для каждого p и n вычисляем тройку (a, b, c) === */
    for (int p = p_start; p <= p_end; p++) {
        fprintf(f, "<tr>\n<td>%d</td>\n<td></td>\n", p);

        for (int n = n_start; n <= n_end; n++) {

            /* a = ceil((n-1)/p) + 1 */
            long long a = ceil_div((long long)(n - 1), (long long)p) + 1;

            /* b = ceil(n/(p-1)); при p=1 знаменатель равен 0 — подставляем 0 */
            long long b = 0;
            if (p > 1) {
                b = ceil_div((long long)n, (long long)(p - 1));
            }

            /*
             * c = минимальное k, при котором:
             *   floor(n/k)*k*(k-1)/2 + (n mod k)*((n mod k)-1)/2
             *       >= ceil( n*(n-1) / (2*p) )
             */
            long long rhs = ceil_div((long long)n * (n - 1), 2LL * p);
            long long c = 1;
            for (long long k = 1; ; k++) {
                long long q = n / k;          /* floor(n/k) */
                long long r = n % k;          /* n mod k    */
                long long lhs = q * k * (k - 1) / 2
                              + r * (r - 1) / 2;
                if (lhs >= rhs) {
                    c = k;
                    break;
                }
            }

            /* --- Определение цвета ячейки --- */
            const char *style = "";
            if (a >= b && a >= c) {
                /* Первое число — наибольшее (или одно из наибольших) */
                style = " style=\"background-color:yellow\"";
            } else if (c > a && c >= b) {
                /* Третье число — строго больше первого и не меньше второго */
                style = " style=\"background-color:green\"";
            } else if (a > b && c > b) {
                /* Второе число — строго наименьшее (оба a и c больше b) */
                style = " style=\"background-color:cyan\"";
            }

            fprintf(f, "<td%s>(%lld,%lld,%lld)</td>\n", style, a, b, c);
        }
        fprintf(f, "</tr>\n");
    }

    fprintf(f, "</table>\n</body>\n</html>\n");
    fclose(f);

    printf("Таблица успешно записана в файл dobro.xls\n");
    return 0;
}
