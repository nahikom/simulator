#include "parallel_final.h"

int main() {
    try {
        ParallelFinal test;
        test.run_complete_test();
    } catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}