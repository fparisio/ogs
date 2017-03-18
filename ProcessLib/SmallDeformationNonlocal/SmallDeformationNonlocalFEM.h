/**
 * \copyright
 * Copyright (c) 2012-2017, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <limits>

#include "MaterialLib/SolidModels/LinearElasticIsotropic.h"
#include "MaterialLib/SolidModels/Lubby2.h"
#include "MaterialLib/SolidModels/Ehlers.h"
#include "MathLib/LinAlg/Eigen/EigenMapTools.h"
#include "NumLib/Fem/FiniteElement/TemplateIsoparametric.h"
#include "NumLib/Fem/ShapeMatrixPolicy.h"
#include "ProcessLib/Deformation/BMatrixPolicy.h"
#include "ProcessLib/Deformation/LinearBMatrix.h"
#include "ProcessLib/LocalAssemblerTraits.h"
#include "ProcessLib/Parameter/Parameter.h"
#include "ProcessLib/Utils/InitShapeMatrices.h"

#include "IntegrationPointData.h"
#include "LocalAssemblerInterface.h"
#include "SmallDeformationNonlocalProcessData.h"

namespace ProcessLib
{
namespace SmallDeformationNonlocal
{
/// Used by for extrapolation of the integration point values. It is ordered
/// (and stored) by integration points.
template <typename ShapeMatrixType>
struct SecondaryData
{
    std::vector<ShapeMatrixType, Eigen::aligned_allocator<ShapeMatrixType>> N;
};

template <typename ShapeFunction, typename IntegrationMethod,
          int DisplacementDim>
class SmallDeformationNonlocalLocalAssembler
    : public SmallDeformationNonlocalLocalAssemblerInterface
{
public:
    using ShapeMatricesType =
        ShapeMatrixPolicyType<ShapeFunction, DisplacementDim>;
    using NodalMatrixType = typename ShapeMatricesType::NodalMatrixType;
    using NodalVectorType = typename ShapeMatricesType::NodalVectorType;
    using ShapeMatrices = typename ShapeMatricesType::ShapeMatrices;
    using BMatricesType = BMatrixPolicyType<ShapeFunction, DisplacementDim>;

    using BMatrixType = typename BMatricesType::BMatrixType;
    using StiffnessMatrixType = typename BMatricesType::StiffnessMatrixType;
    using NodalForceVectorType = typename BMatricesType::NodalForceVectorType;
    using NodalDisplacementVectorType =
        typename BMatricesType::NodalForceVectorType;

    SmallDeformationNonlocalLocalAssembler(
        SmallDeformationNonlocalLocalAssembler const&) = delete;
    SmallDeformationNonlocalLocalAssembler(
        SmallDeformationNonlocalLocalAssembler&&) = delete;

    SmallDeformationNonlocalLocalAssembler(
        MeshLib::Element const& e,
        std::size_t const /*local_matrix_size*/,
        bool is_axially_symmetric,
        unsigned const integration_order,
        SmallDeformationNonlocalProcessData<DisplacementDim>& process_data)
        : _process_data(process_data),
          _integration_method(integration_order),
          _element(e)
    {
        unsigned const n_integration_points =
            _integration_method.getNumberOfPoints();

        _ip_data.reserve(n_integration_points);
        _secondary_data.N.resize(n_integration_points);

        auto const shape_matrices =
            initShapeMatrices<ShapeFunction, ShapeMatricesType,
                              IntegrationMethod, DisplacementDim>(
                e, is_axially_symmetric, _integration_method);

        for (unsigned ip = 0; ip < n_integration_points; ip++)
        {
            _ip_data.emplace_back(*_process_data.material);
            auto& ip_data = _ip_data[ip];
            auto const& sm = shape_matrices[ip];
            ip_data._detJ = sm.detJ;
            ip_data._integralMeasure = sm.integralMeasure;
            ip_data._b_matrices.resize(
                KelvinVectorDimensions<DisplacementDim>::value,
                ShapeFunction::NPOINTS * DisplacementDim);

            auto const x_coord =
                interpolateXCoordinate<ShapeFunction, ShapeMatricesType>(e,
                                                                         sm.N);
            LinearBMatrix::computeBMatrix<DisplacementDim,
                                          ShapeFunction::NPOINTS>(
                sm.dNdx, ip_data._b_matrices, is_axially_symmetric, sm.N,
                x_coord);

            ip_data._sigma.resize(
                KelvinVectorDimensions<DisplacementDim>::value);
            ip_data._sigma_prev.resize(
                KelvinVectorDimensions<DisplacementDim>::value);
            ip_data._eps.resize(KelvinVectorDimensions<DisplacementDim>::value);
            ip_data._eps_prev.resize(
                KelvinVectorDimensions<DisplacementDim>::value);
            ip_data._C.resize(KelvinVectorDimensions<DisplacementDim>::value,
                              KelvinVectorDimensions<DisplacementDim>::value);

            _secondary_data.N[ip] = shape_matrices[ip].N;
        }
    }

    double alpha_0(double const distance2, double const internal_length) const
    {
        double const internal_length2 = internal_length * internal_length;
        return (distance2 > internal_length2)
                   ? 0
                   : (1 - distance2 / (internal_length2)) *
                         (1 - distance2 / (internal_length2));
    }

    void nonlocal(std::size_t const /*mesh_item_id*/,
                  std::vector<std::unique_ptr<
                      SmallDeformationNonlocalLocalAssemblerInterface>> const&
                      local_assemblers) override
    {
        //std::cout << "\nXXX";
        //std::cout << "nonlocal in element " << _element.getID() << "\n";

        unsigned const n_integration_points =
            _integration_method.getNumberOfPoints();

        //
        // For every integration point in this element collect the neighbouring
        // integration points falling in given radius (internal length) and
        // compute the alpha_kl weight.
        //
        for (unsigned k = 0; k < n_integration_points; k++)
        {
            //
            // Collect the integration points.
            //

            //std::cout << "\n\tip = " << k << "\n";
            // For all neighbors of element
            for (auto const& la : local_assemblers)
            {
                auto const xyz = getSingleIntegrationPointCoordinates(k);
                // std::cout << "Current ip_k coords : " << xyz << "\n";
                auto const neighbor_ip_coords =
                    la->getIntegrationPointCoordinates(xyz, 0.025);
                for (auto const& n : neighbor_ip_coords)
                {
                    // output
                    //std::cout << "\t[" << std::get<0>(n) << ", "
                    //          << std::get<1>(n) << ", (";
                    //for (int i = 0; i < std::get<2>(n).size(); ++i)
                    //    std::cout << std::get<2>(n)[i] << ", ";
                    //std::cout << "), " << std::get<3>(n) << "]\n";

                    // save into current ip_k
                    //std::cout << _ip_data[k].non_local_assemblers.size();
                    _ip_data[k].non_local_assemblers.push_back(std::make_tuple(
                        la.get(), std::get<1>(n), std::get<3>(n),
                        std::numeric_limits<double>::quiet_NaN()));
                }
            }

            //
            // Calculate alpha_kl =
            //       alpha_0(|x_k - x_l|) / int_{m \in ip} alpha_0(|x_k - x_m|)
            //
            for (auto& tuple : _ip_data[k].non_local_assemblers)
            {
                //auto const& la_l =
                //    *static_cast<SmallDeformationNonlocalLocalAssembler<
                //        ShapeFunction, IntegrationMethod,
                //        DisplacementDim> const* const>(std::get<0>(tuple));

                //int const l_ele = la_l._element.getID();
                //int const l = std::get<1>(tuple);
                double const distance2_l = std::get<2>(tuple);

                //std::cout << "Compute a_kl for k = " << k << " and l = ("
                //          << l_ele << ", " << l
                //          << "); distance^2_l = " << distance2_l << "\n";

                double a_k_sum_m = 0;
                for (auto const& tuple_m : _ip_data[k].non_local_assemblers)
                {
                    auto const& la_m =
                        *static_cast<SmallDeformationNonlocalLocalAssembler<
                            ShapeFunction, IntegrationMethod,
                            DisplacementDim> const* const>(std::get<0>(tuple_m));

                    //int const m_ele = la_m._element.getID();
                    int const m = std::get<1>(tuple_m);
                    double const distance2_m = std::get<2>(tuple_m);

                    auto const& w_m =
                        la_m._integration_method.getWeightedPoint(m)
                            .getWeight();
                    auto const& detJ_m = la_m._ip_data[m]._detJ;
                    auto const& integralMeasure_m =
                        la_m._ip_data[m]._integralMeasure;

                    a_k_sum_m += w_m * detJ_m * integralMeasure_m *
                                 alpha_0(distance2_m, 0.025);
                    //std::cout
                    //    << "\tCompute sum_a_km for k = " << k << " and m = ("
                    //    << m_ele << ", " << m
                    //    << "); distance^2_m = " << distance2_m
                    //    << "alpha_0(d^2_m, 0.025) = " << alpha_0(distance2_m, 0.025)
                    //    << "; sum_alpha_km = " << a_k_sum_m << "\n";
                }
                double const a_kl = alpha_0(distance2_l, 0.025) / a_k_sum_m;

                //std::cout << "alpha_0(d^2_l, 0.025) = " << alpha_0(distance2_l, 0.025)
                //          << "\n";
                //std::cout << "alpha_kl = " << a_kl << "done\n";
                std::get<3>(tuple) = a_kl;
            }
        }

    }

    Eigen::Vector3d getSingleIntegrationPointCoordinates(
        int integration_point) const
    {
        auto const& N = _secondary_data.N[integration_point];

        Eigen::Vector3d xyz;    // Resulting coordinates
        auto* nodes = _element.getNodes();
        for (int i = 0; i < N.size(); ++i)
        {
            Eigen::Map<Eigen::Vector3d const> node_coordinates{
                nodes[i]->getCoords(), 3};
            xyz += node_coordinates * N[i];
        }

        //std::cout << " xyz = " << xyz << "\n";
        return xyz;
    }

    /// \returns for each of the current element's integration points the
    /// element's id, the integration point number, its coordinates, and the
    /// squared distance from the current integration point.
    std::vector<std::tuple<int, int, Eigen::Vector3d, double>>
    getIntegrationPointCoordinates(Eigen::Vector3d const& coords,
                                   double const internal_length) const override
    {
        unsigned const n_integration_points =
            _integration_method.getNumberOfPoints();

        std::vector<std::tuple<int, int, Eigen::Vector3d, double>> result;
        result.reserve(n_integration_points);

        for (unsigned ip = 0; ip < n_integration_points; ip++)
        {
            //std::cout << _element.getID() << ", " << ip << "\n";

            auto const xyz = getSingleIntegrationPointCoordinates(ip);
            double const distance2 = (xyz - coords).squaredNorm();
            if (distance2 < internal_length * internal_length)
                result.emplace_back(_element.getID(), ip, xyz, distance2);
        }
        //std::cout << "for element " << _element.getID() << " got "
        //          << result.size() << " point in internal_length\n";
        return result;
    }

    void assemble(double const /*t*/, std::vector<double> const& /*local_x*/,
                  std::vector<double>& /*local_M_data*/,
                  std::vector<double>& /*local_K_data*/,
                  std::vector<double>& /*local_b_data*/) override
    {
        OGS_FATAL(
            "SmallDeformationNonlocalLocalAssembler: assembly without jacobian is not "
            "implemented.");
    }

    void preAssemble(double const t,
                     std::vector<double> const& local_x) override
    {
        //auto const local_matrix_size = local_x.size();

        //auto local_Jac = MathLib::createZeroedMatrix<StiffnessMatrixType>(
        //    local_Jac_data, local_matrix_size, local_matrix_size);

        unsigned const n_integration_points =
            _integration_method.getNumberOfPoints();

        SpatialPosition x_position;
        x_position.setElementID(_element.getID());

        for (unsigned ip = 0; ip < n_integration_points; ip++)
        {
            x_position.setIntegrationPoint(ip);

            auto const& B = _ip_data[ip]._b_matrices;
            auto const& eps_prev = _ip_data[ip]._eps_prev;
            auto const& sigma_prev = _ip_data[ip]._sigma_prev;

            auto& eps = _ip_data[ip]._eps;
            auto& sigma = _ip_data[ip]._sigma;
            auto& C = _ip_data[ip]._C;
            auto& material_state_variables =
                *_ip_data[ip]._material_state_variables;

            eps.noalias() =
                B *
                Eigen::Map<typename BMatricesType::NodalForceVectorType const>(
                    local_x.data(), ShapeFunction::NPOINTS * DisplacementDim);

            // sigma is for plastic part only.
            if (!_ip_data[ip]._solid_material.computeConstitutiveRelation(
                    t, x_position, _process_data.dt, eps_prev, eps, sigma_prev,
                    sigma, C, material_state_variables))
                OGS_FATAL("Computation of local constitutive relation failed.");
        }
    }

    void assembleWithJacobian(double const t,
                              std::vector<double> const& local_x,
                              std::vector<double> const& /*local_xdot*/,
                              const double /*dxdot_dx*/, const double /*dx_dx*/,
                              std::vector<double>& /*local_M_data*/,
                              std::vector<double>& /*local_K_data*/,
                              std::vector<double>& local_b_data,
                              std::vector<double>& local_Jac_data) override
    {
        auto const local_matrix_size = local_x.size();

        auto local_Jac = MathLib::createZeroedMatrix<StiffnessMatrixType>(
            local_Jac_data, local_matrix_size, local_matrix_size);

        auto local_b = MathLib::createZeroedVector<NodalDisplacementVectorType>(
            local_b_data, local_matrix_size);

        unsigned const n_integration_points =
            _integration_method.getNumberOfPoints();

        SpatialPosition x_position;
        x_position.setElementID(_element.getID());

        for (unsigned ip = 0; ip < n_integration_points; ip++)
        {
            x_position.setIntegrationPoint(ip);
            auto const& wp = _integration_method.getWeightedPoint(ip);
            auto const& detJ = _ip_data[ip]._detJ;
            auto const& integralMeasure = _ip_data[ip]._integralMeasure;

            auto const& B = _ip_data[ip]._b_matrices;
            //auto const& eps_prev = _ip_data[ip]._eps_prev;
            //auto const& sigma_prev = _ip_data[ip]._sigma_prev;

            auto& eps = _ip_data[ip]._eps;
            auto& sigma = _ip_data[ip]._sigma;
            auto& C = _ip_data[ip]._C;
            //auto& material_state_variables =
            //    *_ip_data[ip]._material_state_variables;

            eps.noalias() =
                B *
                Eigen::Map<typename BMatricesType::NodalForceVectorType const>(
                    local_x.data(), ShapeFunction::NPOINTS * DisplacementDim);

            /*
            if (!_ip_data[ip]._solid_material.updateNonlocalDamage(
                    t, x_position, _process_data.dt, eps_prev, eps, sigma_prev,
                    sigma, C, material_state_variables))
                OGS_FATAL("Computation of non-local damage update failed.");
            */

            {  // Integrate one-function.
                double test_alpha = 0;

                for (auto const& tuple : _ip_data[ip].non_local_assemblers)
                {
                    double const a_kl = std::get<3>(tuple);
                    test_alpha +=
                        a_kl * detJ * wp.getWeight() * integralMeasure;
                }
                assert(std::abs(test_alpha - 1) < 2.7e-15);
            }
            {
                double nonlocal_kappa_d = 0;

                for (auto const& tuple : _ip_data[ip].non_local_assemblers)
                {
                    // Get damage from the local assembler and its corresponding
                    // integration point l.
                    int const& l = std::get<1>(tuple);
                    double const kappa_d =
                        static_cast<SmallDeformationNonlocalLocalAssembler<
                            ShapeFunction, IntegrationMethod,
                            DisplacementDim> const* const>(std::get<0>(tuple))
                            ->_ip_data[l]
                            .getLocalVariable();
                    //std::cerr << kappa_d << "\n";
                    double const a_kl = std::get<3>(tuple);

                    nonlocal_kappa_d += a_kl * kappa_d * detJ * wp.getWeight() *
                                        integralMeasure;
                }
                //std::cerr << "XX " << nonlocal_kappa_d << "\n";

                _ip_data[ip]._damage =
                    _ip_data[ip].updateDamage(t, x_position, nonlocal_kappa_d);
                //std::cerr << "DD " << damage << "\n\n";

                sigma = sigma * (1 - _ip_data[ip]._damage);
            }

            local_b.noalias() -=
                B.transpose() * sigma * detJ * wp.getWeight() * integralMeasure;
            local_Jac.noalias() +=
                B.transpose() * C * B * detJ * wp.getWeight() * integralMeasure;
        }
    }

    void preTimestepConcrete(std::vector<double> const& /*local_x*/,
                             double const /*t*/,
                             double const /*delta_t*/) override
    {
        unsigned const n_integration_points =
            _integration_method.getNumberOfPoints();

        for (unsigned ip = 0; ip < n_integration_points; ip++)
        {
            _ip_data[ip].pushBackState();
        }
    }

    Eigen::Map<const Eigen::RowVectorXd> getShapeMatrix(
        const unsigned integration_point) const override
    {
        auto const& N = _secondary_data.N[integration_point];

        // assumes N is stored contiguously in memory
        return Eigen::Map<const Eigen::RowVectorXd>(N.data(), N.size());
    }

    std::vector<double> const& getNodalValues(
        std::vector<double>& nodal_values) const override
    {
        nodal_values.clear();
        auto local_b = MathLib::createZeroedVector<NodalDisplacementVectorType>(
            nodal_values, ShapeFunction::NPOINTS * DisplacementDim);

        unsigned const n_integration_points =
            _integration_method.getNumberOfPoints();

        SpatialPosition x_position;
        x_position.setElementID(_element.getID());

        for (unsigned ip = 0; ip < n_integration_points; ip++)
        {
            x_position.setIntegrationPoint(ip);
            auto const& wp = _integration_method.getWeightedPoint(ip);
            auto const& detJ = _ip_data[ip]._detJ;
            auto const& integralMeasure = _ip_data[ip]._integralMeasure;

            auto const& B = _ip_data[ip]._b_matrices;
            auto& sigma = _ip_data[ip]._sigma;

            local_b.noalias() +=
                B.transpose() * sigma * detJ * wp.getWeight() * integralMeasure;
        }

        return nodal_values;
    }

    std::vector<double> const& getIntPtDamage(
        std::vector<double>& cache) const override
    {
        cache.clear();
        cache.reserve(_ip_data.size());

        for (auto const& ip_data : _ip_data)
        {
            cache.push_back(ip_data._damage);
        }

        return cache;
    }

    std::vector<double> const& getIntPtSigmaXX(
        std::vector<double>& cache) const override
    {
        return getIntPtSigma(cache, 0);
    }

    std::vector<double> const& getIntPtSigmaYY(
        std::vector<double>& cache) const override
    {
        return getIntPtSigma(cache, 1);
    }

    std::vector<double> const& getIntPtSigmaZZ(
        std::vector<double>& cache) const override
    {
        return getIntPtSigma(cache, 2);
    }

    std::vector<double> const& getIntPtSigmaXY(
        std::vector<double>& cache) const override
    {
        return getIntPtSigma(cache, 3);
    }

    std::vector<double> const& getIntPtSigmaXZ(
        std::vector<double>& cache) const override
    {
        assert(DisplacementDim == 3);
        return getIntPtSigma(cache, 4);
    }

    std::vector<double> const& getIntPtSigmaYZ(
        std::vector<double>& cache) const override
    {
        assert(DisplacementDim == 3);
        return getIntPtSigma(cache, 5);
    }

    std::vector<double> const& getIntPtEpsilonXX(
        std::vector<double>& cache) const override
    {
        return getIntPtEpsilon(cache, 0);
    }

    std::vector<double> const& getIntPtEpsilonYY(
        std::vector<double>& cache) const override
    {
        return getIntPtEpsilon(cache, 1);
    }

    std::vector<double> const& getIntPtEpsilonZZ(
        std::vector<double>& cache) const override
    {
        return getIntPtEpsilon(cache, 2);
    }

    std::vector<double> const& getIntPtEpsilonXY(
        std::vector<double>& cache) const override
    {
        return getIntPtEpsilon(cache, 3);
    }

    std::vector<double> const& getIntPtEpsilonXZ(
        std::vector<double>& cache) const override
    {
        assert(DisplacementDim == 3);
        return getIntPtEpsilon(cache, 4);
    }

    std::vector<double> const& getIntPtEpsilonYZ(
        std::vector<double>& cache) const override
    {
        assert(DisplacementDim == 3);
        return getIntPtEpsilon(cache, 5);
    }

private:
    std::vector<double> const& getIntPtSigma(std::vector<double>& cache,
                                             std::size_t const component) const
    {
        cache.clear();
        cache.reserve(_ip_data.size());

        for (auto const& ip_data : _ip_data)
        {
            if (component < 3)  // xx, yy, zz components
                cache.push_back(ip_data._sigma[component]);
            else  // mixed xy, yz, xz components
                cache.push_back(ip_data._sigma[component] / std::sqrt(2));
        }

        return cache;
    }

    std::vector<double> const& getIntPtEpsilon(
        std::vector<double>& cache, std::size_t const component) const
    {
        cache.clear();
        cache.reserve(_ip_data.size());

        for (auto const& ip_data : _ip_data)
        {
            cache.push_back(ip_data._eps[component]);
        }

        return cache;
    }

    SmallDeformationNonlocalProcessData<DisplacementDim>& _process_data;

    std::vector<IntegrationPointData<BMatricesType, DisplacementDim>,
                Eigen::aligned_allocator<
                    IntegrationPointData<BMatricesType, DisplacementDim>>>
        _ip_data;

    IntegrationMethod _integration_method;
    MeshLib::Element const& _element;
    SecondaryData<typename ShapeMatrices::ShapeType> _secondary_data;
};

template <typename ShapeFunction, typename IntegrationMethod,
          unsigned GlobalDim, int DisplacementDim>
class LocalAssemblerData final
    : public SmallDeformationNonlocalLocalAssembler<
          ShapeFunction, IntegrationMethod, DisplacementDim>
{
public:
    LocalAssemblerData(LocalAssemblerData const&) = delete;
    LocalAssemblerData(LocalAssemblerData&&) = delete;

    LocalAssemblerData(
        MeshLib::Element const& e,
        std::size_t const local_matrix_size,
        bool is_axially_symmetric,
        unsigned const integration_order,
        SmallDeformationNonlocalProcessData<DisplacementDim>& process_data)
        : SmallDeformationNonlocalLocalAssembler<
              ShapeFunction, IntegrationMethod, DisplacementDim>(
              e, local_matrix_size, is_axially_symmetric, integration_order,
              process_data)
    {
    }
};

}  // namespace SmallDeformationNonlocal
}  // namespace ProcessLib