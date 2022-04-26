// unit tests for utils:: functions requiring a LAMMPS

#include "error.h"
#include "input.h"
#include "lammps.h"
#include "memory.h"
#include "utils.h"

#include "../testing/core.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <string>

// whether to print verbose output (i.e. not capturing LAMMPS screen output).
bool verbose = false;

namespace LAMMPS_NS {

class Advanced_utils : public LAMMPSTest {
protected:
    Error *error;

    void SetUp() override
    {
        testbinary = "AdvancedUtils";
        LAMMPSTest::SetUp();
        error = lmp->error;
    }

    void atomic_system()
    {
        BEGIN_HIDE_OUTPUT();
        command("units real");
        command("lattice sc 1.0 origin 0.125 0.125 0.125");
        command("region box block -2 2 -2 2 -2 2");
        command("create_box 8 box");
        command("create_atoms 1 box");
        command("pair_style zero 3.5");
        command("pair_coeff * *");
        command("mass * 1.0");
        command("region left block -2.0 -1.0 INF INF INF INF");
        command("region right block 0.5  2.0 INF INF INF INF");
        command("region top block INF INF -2.0 -1.0 INF INF");
        command("set region left type 2");
        command("set region right type 3");
        END_HIDE_OUTPUT();
    }
};

TEST_F(Advanced_utils, missing_cmd_args)
{
    auto output = CAPTURE_OUTPUT([&] {
        utils::missing_cmd_args(FLERR, "dummy", nullptr);
    });
    EXPECT_EQ(output, "");

    TEST_FAILURE("ERROR: Illegal dummy command: missing argument",
                 utils::missing_cmd_args(FLERR, "dummy", error););
};

TEST_F(Advanced_utils, logmesg)
{
    auto output = CAPTURE_OUTPUT([&] {
        utils::logmesg(lmp, "test message");
    });
    EXPECT_EQ(output, "test message");

    output = CAPTURE_OUTPUT([&] {
        utils::logmesg(lmp, "test message from test {}", testbinary);
    });
    EXPECT_EQ(output, "test message from test " + testbinary);
};

// death tests only. the other cases are tested in the basic utils unit tester
TEST_F(Advanced_utils, bounds_int_fail)
{
    int nlo, nhi;
    TEST_FAILURE("ERROR: Invalid range string: 1x ",
                 utils::bounds(FLERR, "1x", 1, 10, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: -1 ",
                 utils::bounds(FLERR, "-1", 1, 10, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: \\+1 ",
                 utils::bounds(FLERR, "+1", 1, 10, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: 1:3 ",
                 utils::bounds(FLERR, "1:3", 1, 10, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: \\? ",
                 utils::bounds(FLERR, "?", -10, 5, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: 3\\*:2 ",
                 utils::bounds(FLERR, "3*:2", -10, 5, nlo, nhi, error););
}

TEST_F(Advanced_utils, bounds_bigint_fail)
{
    bigint nlo, nhi;
    TEST_FAILURE("ERROR: Invalid range string: 1x ",
                 utils::bounds(FLERR, "1x", 1, 10, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: -1 ",
                 utils::bounds(FLERR, "-1", 1, 10, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: \\+1 ",
                 utils::bounds(FLERR, "+1", 1, 10, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: 1:3 ",
                 utils::bounds(FLERR, "1:3", 1, 10, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: \\? ",
                 utils::bounds(FLERR, "?", -10, 5, nlo, nhi, error););
    TEST_FAILURE("ERROR: Invalid range string: 3\\*:2 ",
                 utils::bounds(FLERR, "3*:2", -10, 5, nlo, nhi, error););
}

TEST_F(Advanced_utils, expand_args)
{
    atomic_system();
    BEGIN_CAPTURE_OUTPUT();
    command("compute temp all temp");
    command("variable temp vector c_temp");
    command("variable step equal step");
    command("variable pe equal pe");
    command("variable pe equal pe");
    command("variable epair equal epair");
    command("compute gofr all rdf 20 1 1 1 2");
    command("fix 1 all ave/time 1 1 1 v_step v_pe v_epair");
    command("fix 2 all nve");
    command("run 1 post no");
    auto output = END_CAPTURE_OUTPUT();

    char **args, **earg;
    constexpr int oarg = 9;
    args    = new char *[oarg];
    args[0] = utils::strdup("v_step");
    args[1] = utils::strdup("c_temp");
    args[2] = utils::strdup("f_1[2*]");
    args[3] = utils::strdup("c_temp[2*4]");
    args[4] = utils::strdup("v_temp[*4]");
    args[5] = utils::strdup("c_rdf[*]");
    args[6] = utils::strdup("c_rdf[1][*]");
    args[7] = utils::strdup("c_rdf[*][2]");
    args[8] = utils::strdup("c_rdf[*][*]");

    auto narg = utils::expand_args(FLERR, oarg, args, 0, earg, lmp);
    EXPECT_EQ(narg, 12);
    EXPECT_STREQ(earg[0], "v_step");
    EXPECT_STREQ(earg[1], "c_temp");
    EXPECT_STREQ(earg[2], "f_1[2]");
    EXPECT_STREQ(earg[3], "f_1[3]");
    EXPECT_STREQ(earg[4], "c_temp[2]");
    EXPECT_STREQ(earg[5], "c_temp[3]");
    EXPECT_STREQ(earg[6], "c_temp[4]");
    EXPECT_STREQ(earg[7], "v_temp[*4]");
    EXPECT_STREQ(earg[8], "c_rdf[*]");
    EXPECT_STREQ(earg[9], "c_rdf[1][*]");
    EXPECT_STREQ(earg[10], "c_rdf[*][2]");
    EXPECT_STREQ(earg[11], "c_rdf[*][*]");

    for (int i = 0; i < narg; ++i)
        delete[] earg[i];
    lmp->memory->sfree(earg);

    narg = utils::expand_args(FLERR, oarg, args, 1, earg, lmp);
    EXPECT_EQ(narg, oarg);
    EXPECT_NE(args, earg);
    EXPECT_STREQ(earg[0], "v_step");
    EXPECT_STREQ(earg[1], "c_temp");
    EXPECT_STREQ(earg[2], "f_1[2*]");
    EXPECT_STREQ(earg[3], "c_temp[2*4]");
    EXPECT_STREQ(earg[4], "v_temp[*4]");
    EXPECT_STREQ(earg[5], "c_rdf[*]");
    EXPECT_STREQ(earg[6], "c_rdf[1][*]");
    EXPECT_STREQ(earg[7], "c_rdf[*][2]");
    EXPECT_STREQ(earg[8], "c_rdf[*][*]");

    for (int i = 0; i < oarg; ++i)
        delete[] args[i];
    delete[] args;
    for (int i = 0; i < narg; ++i)
        delete[] earg[i];
    lmp->memory->sfree(earg);
}

} // namespace LAMMPS_NS

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    ::testing::InitGoogleMock(&argc, argv);

    if (platform::mpi_vendor() == "Open MPI" && !LAMMPS_NS::Info::has_exceptions())
        std::cout << "Warning: using OpenMPI without exceptions. Death tests will be skipped\n";

    // handle arguments passed via environment variable
    if (const char *var = getenv("TEST_ARGS")) {
        std::vector<std::string> env = utils::split_words(var);
        for (auto arg : env) {
            if (arg == "-v") {
                verbose = true;
            }
        }
    }

    if ((argc > 1) && (strcmp(argv[1], "-v") == 0)) verbose = true;

    int rv = RUN_ALL_TESTS();
    MPI_Finalize();
    return rv;
}