#include "gtest/gtest.h"
#include "b_tree_disk.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

template<typename tkey, typename tvalue>
bool compare_results(
    const std::vector<std::pair<tkey, tvalue>>& expected,
    const std::vector<std::pair<tkey, tvalue>>& actual)
{
    if (expected.size() != actual.size()) return false;
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i].first != actual[i].first || expected[i].second != actual[i].second) {
            return false;
        }
    }
    return true;
}

class BTreeDiskTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file = "test_tree";
        fs::remove(test_file + ".tree");
        fs::remove(test_file + ".data");
    }

    void TearDown() override {
        fs::remove(test_file + ".tree");
        fs::remove(test_file + ".data");
    }

    std::string test_file;
};

TEST_F(BTreeDiskTest, InsertAndFind) {
    std::cout << "\n_______________Тест вставки и поиска_______________\n";
    B_tree_disk<IntSerial, StrSerial> tree(test_file);

    std::cout << "Вставляем ключи 1 и 2...\n";
    EXPECT_TRUE(tree.insert({IntSerial{1}, StrSerial{"a"}}));
    EXPECT_TRUE(tree.insert({IntSerial{2}, StrSerial{"b"}}));

    std::cout << "Ищем ключ 1...\n";
    auto val = tree.at(IntSerial{1});
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(val.value().get(), "a");
    std::cout << "_______________Тест завершен_______________\n";
}

TEST_F(BTreeDiskTest, RangeSearch) {
    std::cout << "\n_______________Тест диапазонного поиска_______________\n";
    B_tree_disk<IntSerial, StrSerial> tree(test_file);

    std::cout << "Вставляем ключи 1-4...\n";
    tree.insert({IntSerial{1}, StrSerial{"a"}});
    tree.insert({IntSerial{2}, StrSerial{"b"}});
    tree.insert({IntSerial{3}, StrSerial{"c"}});
    tree.insert({IntSerial{4}, StrSerial{"d"}});

    std::cout << "Ищем диапазон 2-4...\n";
    auto [begin, end] = tree.find_range(IntSerial{2}, IntSerial{4});
    std::vector<std::pair<IntSerial, StrSerial>> result;
    while (begin != end) {
        result.push_back(*begin);
        ++begin;
    }

    std::vector<std::pair<IntSerial, StrSerial>> expected = {
        {IntSerial{2}, StrSerial{"b"}},
        {IntSerial{3}, StrSerial{"c"}},
        {IntSerial{4}, StrSerial{"d"}}
    };

    ASSERT_EQ(result.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(result[i].first.data, expected[i].first.data);
        EXPECT_EQ(result[i].second.data, expected[i].second.data);
    }
    std::cout << "_______________Тест завершен_______________\n";
}

TEST_F(BTreeDiskTest, Persistence) {
    std::cout << "\n_______________Тест персистентности_______________\n";
    {
        std::cout << "Создаем и заполняем дерево...\n";
        B_tree_disk<IntSerial, StrSerial> tree(test_file);
        tree.insert({IntSerial{1}, StrSerial{"a"}});
        tree.insert({IntSerial{2}, StrSerial{"b"}});
    }

    std::cout << "Загружаем дерево из файла...\n";
    B_tree_disk<IntSerial, StrSerial> tree1(test_file);

    EXPECT_TRUE(tree1.at(IntSerial{1}).has_value());
    EXPECT_TRUE(tree1.at(IntSerial{2}).has_value());
    std::cout << "_______________Тест завершен_______________\n";
}

TEST_F(BTreeDiskTest, ComplexStructure) {
    std::cout << "\n_______________Тест сложной структуры_______________\n";
    B_tree_disk<StrSerial, StrSerial, std::less<StrSerial>, 3> tree(test_file);

    std::cout << "Вставляем 100 элементов...\n";
    for (int i = 0; i < 100; ++i) {
        tree.insert({StrSerial{std::to_string(i)}, StrSerial{"value_" + std::to_string(i)}});
    }

    std::cout << "Проверяем все элементы...\n";
    for (int i = 0; i < 100; ++i) {
        auto val = tree.at(StrSerial{std::to_string(i)});
        EXPECT_TRUE(val.has_value());
        EXPECT_EQ(val.value().get(), "value_" + std::to_string(i));
    }
    std::cout << "_______________Тест завершен_______________\n";
}

TEST_F(BTreeDiskTest, EraseTest) {
    std::cout << "\n_______________Тест удаления_______________\n";
    B_tree_disk<IntSerial, StrSerial, std::less<IntSerial>, 3> tree(test_file);

    std::cout << "1. Вставляем 50 элементов...\n";
    for (int i = 0; i < 50; i++) {
        tree.insert({IntSerial{i}, StrSerial{"value_" + std::to_string(i)}});
    }

    std::cout << "\nСостояние дерева до удалений:\n";
    tree.print();
    std::cout << "________________________________________\n";

    std::cout << "\n2. Удаляем ключ 20...\n";
    EXPECT_TRUE(tree.erase(IntSerial{20}));
    EXPECT_FALSE(tree.at(IntSerial{20}).has_value());

    std::cout << "\nСостояние после удаления 20:\n";
    tree.print();
    std::cout << "________________________________________\n";

    std::cout << "\n3. Пытаемся удалить несуществующий ключ 100...\n";
    EXPECT_FALSE(tree.erase(IntSerial{100}));

    std::cout << "\nСостояние после попытки удаления 100:\n";
    tree.print();
    std::cout << "________________________________________\n";

    std::cout << "\n4. Удаляем ключ 5 (должен вызвать перебалансировку)...\n";
    EXPECT_TRUE(tree.at(IntSerial{5}).has_value());
    EXPECT_TRUE(tree.erase(IntSerial{5}));
    EXPECT_FALSE(tree.at(IntSerial{5}).has_value());

    std::cout << "\nСостояние после удаления 5:\n";
    tree.print();
    std::cout << "________________________________________\n";

    std::cout << "\n5. Удаляем ключи 10-14 для проверки перестройки дерева...\n";
    for (int i = 10; i < 15; i++) {
        EXPECT_TRUE(tree.erase(IntSerial{i}));
        EXPECT_FALSE(tree.at(IntSerial{i}).has_value());
    }

    std::cout << "\nСостояние после удаления 10-14:\n";
    tree.print();
    std::cout << "________________________________________\n";

    std::cout << "\n6. Проверяем оставшиеся ключи...\n";
    EXPECT_TRUE(tree.at(IntSerial{0}).has_value());
    EXPECT_EQ(tree.at(IntSerial{0}).value().data, "value_0");

    std::cout << "\n7. Удаляем ключ 15...\n";
    int root_key = 15;
    EXPECT_TRUE(tree.erase(IntSerial{root_key}));
    EXPECT_FALSE(tree.at(IntSerial{root_key}).has_value());

    std::cout << "\nФинальное состояние дерева:\n";
    tree.print();
    std::cout << "_______________Тест завершен_______________\n";
}

TEST_F(BTreeDiskTest, NegativeTest) {
    std::cout << "\n_______________Негативный тест_______________\n";
    B_tree_disk<IntSerial, StrSerial> tree(test_file);

    std::cout << "Пытаемся удалить несуществующий ключ 42...\n";
    EXPECT_FALSE(tree.erase(IntSerial{42}));

    std::cout << "Пытаемся найти несуществующий ключ 42...\n";
    EXPECT_FALSE(tree.at(IntSerial{42}).has_value());
    std::cout << "_______________Тест завершен_______________\n";
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}