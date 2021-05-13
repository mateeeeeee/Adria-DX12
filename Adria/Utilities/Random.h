#pragma once

#include <random>
#include <cmath>
#include <type_traits>

namespace adria
{

    template<typename FloatType = double,
        typename Generator = std::mt19937,
        typename = std::enable_if_t<std::is_floating_point_v<FloatType>>
    >
    class RealRandomGenerator
    {

    public:
        using ResultType = FloatType;
        using GeneratorType = Generator;
        using DistributionType = std::uniform_real_distribution<FloatType>;

    public:

        explicit RealRandomGenerator(Generator&& _eng
            = Generator{ std::random_device{}() }) : eng(std::move(_eng)), dist() {}


        explicit RealRandomGenerator(Generator const& _eng)
            : eng(_eng), dist() {}

        RealRandomGenerator(FloatType min, FloatType max, Generator&& _eng
            = Generator{ std::random_device{}() }) : eng(std::move(_eng)), dist(min,max) {}
       

        ResultType operator()() { return dist(eng); }

        constexpr ResultType Min() const { return dist.min(); }

        constexpr ResultType Max() const { return dist.max(); }

        void ResetState() { dist.reset(); }

    private:
        GeneratorType eng;
        DistributionType dist;
    };


    template<typename IntType = int,
        typename Generator = std::mt19937,
        typename = std::enable_if_t<std::is_integral_v<IntType>>
    >
    class IntRandomGenerator
    {

    public:
        using ResultType = IntType;
        using GeneratorType = Generator;
        using DistributionType = std::uniform_int_distribution<IntType>;

    public:

        explicit IntRandomGenerator(Generator&& _eng
            = Generator{ std::random_device{}() }) : eng(std::move(_eng)), dist() {}


        explicit IntRandomGenerator(Generator const& _eng)
            : eng(_eng), dist() {}

        IntRandomGenerator(IntType min, IntType max, Generator&& _eng
            = Generator{ std::random_device{}() }) : eng(std::move(_eng)), dist(min, max) {}


        ResultType operator()() { return dist(eng); }

        constexpr ResultType Min() const { return dist.min(); }

        constexpr ResultType Max() const { return dist.max(); }

        void ResetState() { dist.reset(); }

    private:
        GeneratorType eng;
        DistributionType dist;
    };
}