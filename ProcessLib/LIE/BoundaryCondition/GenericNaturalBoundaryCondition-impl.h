/**
 * \copyright
 * Copyright (c) 2012-2018, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#pragma once

#include "MeshLib/MeshSearch/NodeSearch.h"  // for getUniqueNodes

#include "ProcessLib/BoundaryCondition/GenericNaturalBoundaryConditionLocalAssembler.h"
#include "ProcessLib/Utils/CreateLocalAssemblers.h"

namespace ProcessLib
{
namespace LIE
{
template <typename BoundaryConditionData,
          template <typename, typename, unsigned>
          class LocalAssemblerImplementation>
template <typename Data>
GenericNaturalBoundaryCondition<BoundaryConditionData,
                                LocalAssemblerImplementation>::
    GenericNaturalBoundaryCondition(
        typename std::enable_if<
            std::is_same<typename std::decay<BoundaryConditionData>::type,
                         typename std::decay<Data>::type>::value,
            bool>::type is_axially_symmetric,
        unsigned const integration_order, unsigned const shapefunction_order,
        NumLib::LocalToGlobalIndexMap const& dof_table_bulk,
        int const variable_id, int const component_id,
        unsigned const global_dim, std::vector<MeshLib::Element*>&& elements,
        Data&& data, FractureProperty const& fracture_prop)
    : _data(std::forward<Data>(data)),
      _elements(std::move(elements)),
      _integration_order(integration_order)
{
    assert(component_id < dof_table_bulk.getNumberOfComponents());

    std::vector<MeshLib::Node*> nodes = MeshLib::getUniqueNodes(_elements);
    DBUG("Found %d nodes for Natural BCs for the variable %d and component %d",
         nodes.size(), variable_id, component_id);

    MeshLib::MeshSubset mesh_subset =
        dof_table_bulk.getMeshSubset(variable_id, component_id);

    // Create local DOF table from intersected mesh subsets for the given
    // variable and component ids.
    _dof_table_boundary.reset(dof_table_bulk.deriveBoundaryConstrainedMap(
        variable_id, {component_id}, std::move(mesh_subset), _elements));

    createLocalAssemblers<LocalAssemblerImplementation>(
        global_dim, _elements, *_dof_table_boundary, shapefunction_order,
        _local_assemblers, is_axially_symmetric, _integration_order, _data,
        fracture_prop, variable_id);
}

template <typename BoundaryConditionData,
          template <typename, typename, unsigned>
          class LocalAssemblerImplementation>
GenericNaturalBoundaryCondition<
    BoundaryConditionData,
    LocalAssemblerImplementation>::~GenericNaturalBoundaryCondition()
{
    for (auto e : _elements)
        delete e;
}

template <typename BoundaryConditionData,
          template <typename, typename, unsigned>
          class LocalAssemblerImplementation>
void GenericNaturalBoundaryCondition<
    BoundaryConditionData,
    LocalAssemblerImplementation>::applyNaturalBC(const double t,
                                                  const GlobalVector& x,
                                                  GlobalMatrix& K,
                                                  GlobalVector& b)
{
    GlobalExecutor::executeMemberOnDereferenced(
        &GenericNaturalBoundaryConditionLocalAssemblerInterface::assemble,
        _local_assemblers, *_dof_table_boundary, t, x, K, b);
}

}  // namespace LIE
}  // namespace ProcessLib
