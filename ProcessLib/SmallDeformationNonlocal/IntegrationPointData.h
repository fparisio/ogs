/**
 * \copyright
 * Copyright (c) 2012-2017, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#pragma once

#include "LocalAssemblerInterface.h"

namespace ProcessLib
{
namespace SmallDeformationNonlocal
{

template <typename BMatricesType, int DisplacementDim>
struct IntegrationPointData final
{
    explicit IntegrationPointData(
        MaterialLib::Solids::MechanicsBase<DisplacementDim>& solid_material)
        : solid_material(solid_material),
          material_state_variables(
              solid_material.createMaterialStateVariables())
    {
        if (auto const msv =
                dynamic_cast<typename MaterialLib::Solids::Ehlers::SolidEhlers<
                    DisplacementDim>::MaterialStateVariables*>(
                    material_state_variables.get()))
        {
            eps_p_V = &msv->eps_p_V;
            eps_p_D_xx = &(msv->eps_p_D[0]);
        }
    }

#if defined(_MSC_VER) && _MSC_VER < 1900
    // The default generated move-ctor is correctly generated for other
    // compilers.
    explicit IntegrationPointData(IntegrationPointData&& other)
        : b_matrices(std::move(other.b_matrices)),
          sigma(std::move(other.sigma)),
          sigma_prev(std::move(other.sigma_prev)),
          eps(std::move(other.eps)),
          eps_prev(std::move(other.eps_prev)),
          solid_material(other.solid_material),
          material_state_variables(std::move(other.material_state_variables)),
          C(std::move(other.C)),
          integration_weight(std::move(other.integration_weight)),
    {
    }
#endif  // _MSC_VER

    typename BMatricesType::BMatrixType b_matrices;
    typename BMatricesType::KelvinVectorType sigma, sigma_prev;
    typename BMatricesType::KelvinVectorType eps, eps_prev;
    double damage = 0;
    double nonlocal_kappa_d = 0;

    MaterialLib::Solids::MechanicsBase<DisplacementDim>& solid_material;
    std::unique_ptr<typename MaterialLib::Solids::MechanicsBase<
        DisplacementDim>::MaterialStateVariables>
        material_state_variables;

    typename BMatricesType::KelvinMatrixType C;
    double integration_weight;

    double const* eps_p_V;
    double const* eps_p_D_xx;

    void pushBackState()
    {
        eps_prev = eps;
        sigma_prev = sigma;
        material_state_variables->pushBackState();
    }

    double getLocalVariable() const
    {
        return material_state_variables->getLocalVariable();
    }

    double updateDamage(double const t, SpatialPosition const& x_position,
                      double const kappa_d)
    {
        return static_cast<
                   MaterialLib::Solids::Ehlers::SolidEhlers<DisplacementDim>&>(
                   solid_material)
            .updateDamage(t, x_position, kappa_d, *material_state_variables);
    }

    std::vector<std::tuple<
        // element's local assembler
        SmallDeformationNonlocalLocalAssemblerInterface const* const,
        int,     // integration point id,
        double,  // squared distance to current integration point
        double   // alpha_kl
        >>
        non_local_assemblers;
};

}  // namespace SmallDeformationNonlocal
}  // namespace ProcessLib