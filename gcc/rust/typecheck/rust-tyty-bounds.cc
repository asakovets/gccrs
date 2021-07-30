// Copyright (C) 2021 Free Software Foundation, Inc.

// This file is part of GCC.

// GCC is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3, or (at your option) any later
// version.

// GCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#include "rust-hir-type-bounds.h"
#include "rust-hir-trait-resolve.h"

namespace Rust {
namespace Resolver {

void
TypeBoundsProbe::scan ()
{
  std::vector<HIR::TypePath *> possible_trait_paths;
  mappings->iterate_impl_blocks (
    [&] (HirId id, HIR::ImplBlock *impl) mutable -> bool {
      // we are filtering for trait-impl-blocks
      if (!impl->has_trait_ref ())
	return true;

      TyTy::BaseType *impl_type = nullptr;
      bool ok
	= context->lookup_type (impl->get_type ()->get_mappings ().get_hirid (),
				&impl_type);
      if (!ok)
	return true;

      if (!receiver->can_eq (impl_type, false))
	return true;

      possible_trait_paths.push_back (impl->get_trait_ref ().get ());
      return true;
    });

  for (auto &trait_path : possible_trait_paths)
    {
      TraitReference &trait_ref = TraitResolver::Resolve (*trait_path);

      if (!trait_ref.is_error ())
	trait_references.push_back (trait_ref);
    }
}

TraitReference &
TypeCheckBase::resolve_trait_path (HIR::TypePath &path)
{
  return TraitResolver::Resolve (path);
}

} // namespace Resolver

namespace TyTy {

std::string
TypeBoundPredicate::as_string () const
{
  return reference->as_string ();
}

} // namespace TyTy
} // namespace Rust
