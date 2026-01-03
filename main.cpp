#include "modular.hpp"
#include <iostream>
#include <string>
#include <limits>
#include <functional>

class CalculatorApp {
public:
    void run() {
        std::cout << "=== Калькулятор больших целых чисел (BigInt) ===\n";
        std::cout << "Формат ввода: Hex (шестнадцатеричный, без 0x)\n";

        while (running) {
            printMenu();
            int choice = readValue<int>("Выберите операцию: ");
            executeChoice(choice);
            
            if (running) {
                std::cout << "\nНажмите Enter, чтобы продолжить...";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cin.get();
            }
        }
    }

private:
    bool running = true;

    void printMenu() const {
        std::cout << "\n" << std::string(30, '-') << "\n"
                  << " 1. Сложение (+)       7. В десятичную систему\n"
                  << " 2. Вычитание (-)      8. Сравнение (==, <, >)\n"
                  << " 3. Умножение (*)      9. Сдвиг влево (<<)\n"
                  << " 4. Остаток (%)       10. Сдвиг вправо (>>)\n"
                  << " 5. Возведение в степень (Стандарт)\n"
                  << " 6. Возведение в степень (Монтгомери)\n"
                  << "11. Инфо о числе (Zero/Bits)\n"
                  << " 0. Выход\n"
                  << std::string(30, '-') << "\n";
    }

    // Универсальный метод чтения ввода
    template<typename T>
    T readValue(const std::string& prompt) {
        T value;
        while (true) {
            std::cout << prompt;
            if (std::cin >> value) {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return value;
            }
            std::cout << "Ошибка ввода. Попробуйте снова.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    BigInt readBigInt(const std::string& label) {
        while (true) {
            try {
                std::string input = readValue<std::string>("Введите " + label + " (hex): ");
                return BigInt(input); // Используем конструктор из hex-строки
            } catch (...) {
                std::cout << "Ошибка: Неверный hex-формат.\n";
            }
        }
    }

    void display(const std::string& op, const BigInt& res) {
        std::cout << "\n[Результат " << op << "]\n"
                  << "HEX: 0x" << res.toHex() << "\n"
                  << "DEC: " << res.toDec() << "\n";
    }

    void executeChoice(int choice) {
        try {
            switch (choice) {
                case 0: running = false; break;
                case 1: {
                    BigInt a = readBigInt("a");
                    BigInt b = readBigInt("b");
                    display("a + b", a + b);
                    break;
                }
                case 2: {
                    BigInt a = readBigInt("a");
                    BigInt b = readBigInt("b");
                    display("a - b", a - b);
                    break;
                }
                case 3: {
                    BigInt a = readBigInt("a");
                    BigInt b = readBigInt("b");
                    display("a * b", a * b);
                    break;
                }
                case 4: {
                    BigInt a = readBigInt("a");
                    BigInt b = readBigInt("b");
                    display("a % b", a % b);
                    break;
                }
                case 5: {
                    BigInt b = readBigInt("основание");
                    BigInt e = readBigInt("степень");
                    BigInt m = readBigInt("модуль");
                    display("b^e mod m", mod_exp(b, e, m));
                    break;
                }
                case 6: {
                    BigInt b = readBigInt("основание");
                    BigInt e = readBigInt("степень");
                    BigInt m = readBigInt("модуль (нечетный)");
                    display("b^e mod m (Montgomery)", mod_exp_montgomery(b, e, m));
                    break;
                }
                case 7: {
                    BigInt a = readBigInt("число");
                    std::cout << "Десятичное значение: " << a.toDec() << "\n";
                    break;
                }
                case 8: handleComparison(); break;
                case 9: {
                    BigInt a = readBigInt("число");
                    size_t s = readValue<size_t>("Сдвиг (бит): ");
                    display("a << n", a.shiftLeft(s));
                    break;
                }
                case 10: {
                    BigInt a = readBigInt("число");
                    size_t s = readValue<size_t>("Сдвиг (бит): ");
                    display("a >> n", a.shiftRight(s));
                    break;
                }
                case 11: {
                    BigInt a = readBigInt("число");
                    std::cout << "Это ноль? " << (a.isZero() ? "Да" : "Нет") << "\n";
                    std::cout << "Бит в числе: " << a.bitLength() << "\n";
                    break;
                }
                default: std::cout << "Неверный пункт меню.\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Критическая ошибка: " << e.what() << "\n";
        }
    }

    void handleComparison() {
        BigInt a = readBigInt("a");
        BigInt b = readBigInt("b");
        std::cout << "\nРезультаты сравнения:\n"
                  << "a == b: " << (a == b ? "true" : "false") << "\n"
                  << "a <  b: " << (a < b  ? "true" : "false") << "\n"
                  << "a >  b: " << (a > b  ? "true" : "false") << "\n";
    }
};

int main() {
    CalculatorApp app;
    app.run();
    return 0;
}
