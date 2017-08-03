
//#define VISUALIZE_AST
//#define DUMP_IR

#include "execute/execute.h"
#include "front/parse.h"
#include "back/compile.h"
#include "test_util.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace lwnn;
using namespace lwnn::test_util;

TEST_CASE("basic bool expressions") {
    SECTION("literals") {
        REQUIRE(test<bool>("true;"));
        REQUIRE(!test<bool>("false;"));
    }

    //TO DO:  boolean operators
}

TEST_CASE("basic integer expressions") {

    SECTION("literal ints") {
        REQUIRE(test<int>("1;") == 1);
        REQUIRE(test<int>("-1;") == -1);
        REQUIRE(test<int>("2;") == 2);
        REQUIRE(test<int>("-2;") == -2);
    }

    SECTION("basic binary expressions for each operator") {
        REQUIRE(test<int>("2 + 3;") == 5);
        REQUIRE(test<int>("2 - 3;") == -1);
        REQUIRE(test<int>("2 * 3;") == 6);
        REQUIRE(test<int>("6 / 2;") == 3);
    }

    SECTION("Order of operations") {
        REQUIRE(test<int>("1 + 2 * 3;") == 7);
        REQUIRE(test<int>("6 + 10 / 2;") == 11);
        REQUIRE(test<int>("(1 + 2) * 3;") == 9);
        REQUIRE(test<int>("(5 + 10) / 3;") == 5);
    }
}

TEST_CASE("equality") {
    //Boolean
    REQUIRE(test<bool>("true == true;"));
    REQUIRE(test<bool>("false == false;"));
    REQUIRE(!test<bool>("false == true;"));
    REQUIRE(!test<bool>("true == false;"));

    //Integer
    REQUIRE(test<bool>("1 == 1;"));
    REQUIRE(!test<bool>("1 == 2;"));

    //Float
    REQUIRE(test<bool>("1.0 == 1.0;"));
    REQUIRE(!test<bool>("1.0 == 2.0;"));

    //Integer-float implict casts
    REQUIRE(test<bool>("1 == 1.0;"));
    REQUIRE(!test<bool>("1 == 2.0;"));
    REQUIRE(test<bool>("1.0 == 1;"));
    REQUIRE(!test<bool>("1.0 == 2;"));

    //Integer-bool implict casts?

    //Float-bool implict casts?

}

TEST_CASE("basic float expressions") {
    SECTION("literal floats") {
        REQUIRE(test<float>("1.0;") == 1.0);
        REQUIRE(test<float>("-1.0;") == -1.0);
        REQUIRE(test<float>("2.0;") == 2.0);

        REQUIRE(test<float>("234.0;") == 234.0);
        REQUIRE(test<float>("-2.0;") == -2);
    }
    SECTION("basic binary expressions for each operator") {
        REQUIRE(test<float>("2.0 + 3.0;") == 5.0);
        REQUIRE(test<float>("2.0 - 3.0;") == -1.0);
        REQUIRE(test<float>("2.0 * 3.0;") == 6.0);
        REQUIRE(test<float>("6.0 / 2.0;") == 3.0);
    }

    SECTION("Order of operations") {
        REQUIRE(test<float>("1.0 + 2.0 * 3.0;") == 7.0);
        REQUIRE(test<float>("6.0 + 10.0 / 2.0;") == 11.0);
        REQUIRE(test<float>("(1.0 + 2.0) * 3.0;") == 9.0);
        REQUIRE(test<float>("(5.0 + 10.0) / 3.0;") == 5.0);
    }
}

TEST_CASE("compound expressions") {

    REQUIRE(test<bool>("{ true; }"));
    REQUIRE(!test<bool>("{ false; }"));
    REQUIRE(test<bool>("{ false; true; }"));
    REQUIRE(!test<bool>("{ true; false; }"));

    REQUIRE(test<int>("{ 1; }") == 1);
    REQUIRE(test<int>("{ 2; }") == 2);
    REQUIRE(test<int>("{ 2; 3; }") == 3);
    REQUIRE(test<int>("{ 2; 3; 4; }") == 4);

    REQUIRE(test<float>("{ 1.0; }") == 1.0);
    REQUIRE(test<float>("{ 2.0; }") == 2.0);
    REQUIRE(test<float>("{ 2.0; 3.0; }") == 3.0);
    REQUIRE(test<float>("{ 2.0; 3.0; 4.0; }") == 4.0);
}

TEST_CASE("conditional expressions") {
    REQUIRE(test<int>("(? true, 1, 2);") == 1);
    REQUIRE(test<int>("(? false, 1, 2);") == 2);

    //Implicit casting of true part / false part
    REQUIRE(test<float>("(? true, 1.0, 2);") == 1.0);
    REQUIRE(test<float>("(? false, 1, 2.0);") == 2.0);

    //Implicit casting of condition from int to bool
    REQUIRE(test<int>("(? 1, 2, 3);") == 2);
    REQUIRE(test<int>("(? 2, 2, 3);") == 2);
    REQUIRE(test<int>("(? 0, 2, 3);") == 3);

    //Implicit casting of condition from float to bool
    REQUIRE(test<int>("(? 1.0, 2, 3);") == 2);
    REQUIRE(test<int>("(? 0, 2, 3);") == 3);

    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    REQUIRE(test<int>(ec, "foo:int = (? 1.0, 2, 3);") == 2);
    REQUIRE(test<int>(ec, "foo;") == 2);
    REQUIRE(test<int>(ec, "foo = (? 0, 2, 3);") == 3);
}

TEST_CASE("Nested conditional expressions") {
    //std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    REQUIRE(test<int>("(? true, (? true, 1, 2), 3);") == 1);
    REQUIRE(test<int>("(? true, (? false, 1, 2), 3);") == 2);
    REQUIRE(test<int>("(? false, (? true, 1, 2), 3);") == 3);

    REQUIRE(test<int>("(? true, 1, (? true, 2, 3));") == 1);
    REQUIRE(test<int>("(? false, 1, (? true, 2, 3));") == 2);
    REQUIRE(test<int>("(? false, 1, (? false, 2, 3));") == 3);
}

TEST_CASE("Nested conditional expressions with variables") {
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    exec(ec, "one:int = 1; two:int = 2; three:int = 3; t:bool = true; f:bool = false;");

    REQUIRE(test<int>(ec, "(? t, (? t, one, two), three);") == 1);
    REQUIRE(test<int>(ec, "(? t, (? f, one, two), three);") == 2);
    REQUIRE(test<int>(ec, "(? f, (? t, one, two), three);") == 3);

    REQUIRE(test<int>(ec, "(? t, one, (? t, two, three));") == 1);
    REQUIRE(test<int>(ec, "(? f, one, (? t, two, three));") == 2);
    REQUIRE(test<int>(ec, "(? f, one, (? f, two, three));") == 3);
}

TEST_CASE("if with value") {
    REQUIRE(test<int>("if (true) 1; else 2;") == 1);
    REQUIRE(test<int>("if (true) { 1; } else { 2; }") == 1);
    REQUIRE(test<int>("if (true) { 1; 2; } else { 3; 4; }") == 2);
    REQUIRE(test<int>("if (false) { 1; 2; } else { 3; 4; }") == 4);
}

TEST_CASE("if as rvalue") {
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    exec(ec, "a:int;");

    exec(ec, "a = if (true) 1 else 2;");
    REQUIRE(test<int>(ec, "a;") == 1);
    exec(ec, "a = if (false) 1 else 2;");
    REQUIRE(test<int>(ec, "a;") == 2);

    exec(ec, "a = if (true) { 1; } else { 2; };");
    REQUIRE(test<int>(ec, "a;") == 1);
    exec(ec, "a = if (false) { 1; } else { 2; };");
    REQUIRE(test<int>(ec, "a;") == 2);

    exec(ec, "a = if (true) { 1; 2; } else { 3; 4; };");
    REQUIRE(test<int>(ec, "a;") == 2);
    exec(ec, "a = if (false) { 1; 2; } else { 3; 4; };");
    REQUIRE(test<int>(ec, "a;" ) == 4);
}


TEST_CASE("nested if and else if with values") {
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    exec(ec, "a:int; b:int;");
    const char *TEST_SRC = "if (a == 0) if(b == 0) 1; else 2; else if(b == 0) 3; else 4;";
    exec(ec, "a = 0; b = 0;");
    REQUIRE(test<int>(ec, TEST_SRC) == 1);
    exec(ec, "a = 0; b = 1;");
    REQUIRE(test<int>(ec, TEST_SRC) == 2);
    exec(ec, "a = 1; b = 0;");
    REQUIRE(test<int>(ec, TEST_SRC) == 3);
    exec(ec, "a = 1; b = 1;");
    REQUIRE(test<int>(ec, TEST_SRC) == 4);
}


TEST_CASE("if with effects") {
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    exec(ec, "a:int; b:int;");
    const char *TEST_SRC = "if (a) b = 1; b;";
    REQUIRE(test<int>(ec, TEST_SRC) == 0);
    exec(ec, "a = 1;");
    REQUIRE(test<int>(ec, TEST_SRC) == 1); //Second execution should because a != 0;
}

TEST_CASE("nested if and else if with effects") {
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    exec(ec, "a:int; b:int; c:int;");
    const char *TEST_SRC = "if (a == 0) if(b == 0) c = 1; else c = 2; else if(b == 0) c = 3; else c = 4;";
    exec(ec, "a = 0; b = 0;");
    exec(ec, TEST_SRC);
    REQUIRE(test<int>(ec, "c;") == 1);
    exec(ec, "a = 0; b = 1;");
    exec(ec, TEST_SRC);
    REQUIRE(test<int>(ec, "c;") == 2);
    exec(ec, "a = 1; b = 0;");
    exec(ec, TEST_SRC);
    REQUIRE(test<int>(ec, "c;") == 3);
    exec(ec, "a = 1; b = 1;");
    exec(ec, TEST_SRC);
    REQUIRE(test<int>(ec, "c;") == 4);
}

TEST_CASE("casting") {

    SECTION("implicit casts with literals") {
        //int to float
        REQUIRE(test<float>("1.0 + 1;") == 2.0);
        REQUIRE(test<float>("1 + 1.0;") == 2.0);
    }

    SECTION("implicit cast to bool type") {
        std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
        REQUIRE(!test<bool>(ec, "foo:bool;"));
        REQUIRE(!test<bool>(ec, "foo = 0;"));
        REQUIRE(test<bool>(ec, "foo = 1;"));

        REQUIRE(!test<bool>(ec, "foo = 0.0;"));
        REQUIRE(test<bool>(ec, "foo = 1.0;"));
    }

    //TODO: implicit casts with variables?

    SECTION("explicit casts") {
        //int to bool
        REQUIRE(!test<bool>("cast<bool>(0);"));
        REQUIRE(test<bool>("cast<bool>(1);"));
        REQUIRE(test<bool>("cast<bool>(2);"));

        //float to int
        REQUIRE(test<int>("cast<int>(1.0) + 1;") == 2);
        REQUIRE(test<int>("1 + cast<int>(1.0);") == 2);

        //float to bool
        REQUIRE(test<bool>("cast<bool>(2.1);"));
        REQUIRE(!test<bool>("cast<bool>(0.0);"));
    }
}

TEST_CASE("variable declarations") {
    SECTION("bool variable declarations") {
        REQUIRE(!test<bool>("foo:bool;"));
        REQUIRE(test<bool>("foo:bool = true;"));
    }

    SECTION("int variable declarations") {
        REQUIRE(test<int>("foo:int;") == 0);
        REQUIRE(test<int>("foo:int = 101;") == 101);
    }

    SECTION("float variable declarations") {
        REQUIRE(test<float>("foo:float;") == 0.0);
        REQUIRE(test<float>("foo:float = 101.0;") == 101.0);
    }

    SECTION("initializer casting") {
        REQUIRE(!test<bool>("foo:bool = cast<bool>(0);")); //explicit
        REQUIRE(test<bool>("foo:bool = cast<bool>(1);")); //explicit
        REQUIRE(test<bool>("foo:bool = cast<bool>(2);")); //explicit

        REQUIRE(test<float>("foo:float = 101;") == 101.0);          //implicit
        REQUIRE(test<int>("foo:int = cast<int>(101.0);") == 101.0); //explicit
    }
}

TEST_CASE("variables with initializers persist between REPL evaluations") {
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();

    REQUIRE(test<bool>(ec, "fee:bool= true;"));
    REQUIRE(test<bool>(ec, "fee;"));
    REQUIRE(test<bool>(ec, "fee;"));

    REQUIRE(test<int>(ec, "foo:int = 123;") == 123);
    REQUIRE(test<int>(ec, "foo;") == 123);
    REQUIRE(test<int>(ec, "foo;") == 123);

    REQUIRE(test<float>(ec, "bar:float = 234.0;") == 234.0);
    REQUIRE(test<float>(ec, "bar;") == 234.0);
    REQUIRE(test<float>(ec, "bar;") == 234.0);
}

TEST_CASE("variables without initializers persist between REPL evaluations") {
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();

    REQUIRE(!test<bool>(ec, "fee:bool;"));
    REQUIRE(!test<bool>(ec, "fee;"));
    REQUIRE(!test<bool>(ec, "fee;"));

    REQUIRE(test<int>(ec, "foo:int;") == 0);
    REQUIRE(test<int>(ec, "foo;") == 0);
    REQUIRE(test<int>(ec, "foo;") == 0);

    REQUIRE(test<float>(ec, "bar:float;") == 0);
    REQUIRE(test<float>(ec, "bar;") == 0);
    REQUIRE(test<float>(ec, "bar;") == 0);
}

TEST_CASE("chained variable declaration and assignment") {
    SECTION("int") {
        std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
        REQUIRE(test<int>(ec, "a:int = b:int = c:int = 10;"));
        REQUIRE(test<int>(ec, "a;") == 10);
        REQUIRE(test<int>(ec, "b;") == 10);
        REQUIRE(test<int>(ec, "c;") == 10);
    }
    SECTION("float") {
        std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
        REQUIRE(test<float>(ec, "a:float = b:float = c:float = 1.23;"));
        REQUIRE(test<float>(ec, "a;") == 1.23f);
        REQUIRE(test<float>(ec, "b;") == 1.23f);
        REQUIRE(test<float>(ec, "c;") == 1.23f);
    }
    SECTION("bool") {
        std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
        REQUIRE(test<bool>(ec, "a:bool = b:bool = c:bool = true;"));
        REQUIRE(test<bool>(ec, "a;"));
        REQUIRE(test<bool>(ec, "b;"));
        REQUIRE(test<bool>(ec, "c;"));
    }
}

TEST_CASE("multiple declarations in a module") {
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    auto results = testWithResults(ec, "foo:int = 10; bar:float = 1.1; bat:bool = true;");
    REQUIRE(results.size() == 3);
    REQUIRE(results[0].get<int>() == 10);
    REQUIRE(results[1].get<float>() == 1.1f);
    REQUIRE(results[2].get<bool>());
    REQUIRE(test<int>(ec, "foo;") == 10);
    REQUIRE(test<float>(ec, "bar;") == 1.1f);
    REQUIRE(test<bool>(ec, "bat;"));
}

TEST_CASE("arithmetic with variables") {
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();

    REQUIRE(test<int>(ec, "foo:int = 100;") == 100);
    REQUIRE(test<int>(ec, "foo + 2;") == 102);
    REQUIRE(test<int>(ec, "foo - 2;") == 98);
    REQUIRE(test<int>(ec, "foo * 2;") == 200);
    REQUIRE(test<int>(ec, "foo / 2;") == 50);

    REQUIRE(test<float>(ec, "bar:float = 100.0;") == 100.0);
    REQUIRE(test<float>(ec, "bar + 2.0;") == 102.0);
    REQUIRE(test<float>(ec, "bar - 2.0;") == 98.0);
    REQUIRE(test<float>(ec, "bar * 2.0;") == 200.0);
    REQUIRE(test<float>(ec, "bar / 2.0;") == 50.0);
}

TEST_CASE("simple assignment expressions") {
    //TODO: bool
    //TODO: float
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    REQUIRE(test<int>(ec, "foo:int = 100;") == 100);
    REQUIRE(test<int>(ec, "foo;") == 100);
    REQUIRE(test<int>(ec, "foo = 200;") == 200);
    REQUIRE(test<int>(ec, "foo;") == 200);
    REQUIRE(test<int>(ec, "foo = 201;") == 201);
    REQUIRE(test<int>(ec, "foo;") == 201);
}

TEST_CASE("chained assignment expressions") {
    //TODO: bool
    //TODO: float
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    REQUIRE(test<int>(ec, "foo:int;") == 0);
    REQUIRE(test<int>(ec, "bar:int;") == 0);
    REQUIRE(test<int>(ec, "bat:int;") == 0);
    REQUIRE(test<int>(ec, "foo = bar = 101; ") == 101);
    REQUIRE(test<int>(ec, "foo = bar = bat = 101; ") == 101);
}

TEST_CASE("variable declaration as expression") {
    //TODO: bool
    //TODO: float
    std::shared_ptr<execute::ExecutionContext> ec = execute::createExecutionContext();
    REQUIRE(test<int>(ec, "foo:int = 100;") == 100);
    REQUIRE(test<int>(ec, "foo = bar:int = 102;") == 102);
    REQUIRE(test<int>(ec, "foo;") == 102);
}