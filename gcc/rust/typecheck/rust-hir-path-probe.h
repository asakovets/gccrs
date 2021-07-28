// Copyright (C) 2020 Free Software Foundation, Inc.

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

#ifndef RUST_HIR_PATH_PROBE_H
#define RUST_HIR_PATH_PROBE_H

#include "rust-hir-type-check-base.h"
#include "rust-hir-full.h"
#include "rust-tyty.h"
#include "rust-substitution-mapper.h"
#include "rust-hir-type-bounds.h"

namespace Rust {
namespace Resolver {

struct PathProbeCandidate
{
  enum CandidateType
  {
    IMPL_CONST,
    IMPL_TYPE_ALIAS,
    IMPL_FUNC,

    TRAIT_ITEM_CONST,
    TRAIT_TYPE_ALIAS,
    TRAIT_FUNC,
  };

  struct ImplItemCandidate
  {
    HIR::ImplItem *impl_item;
    HIR::ImplBlock *parent;
  };

  struct TraitItemCandidate
  {
    TraitReference &trait_ref;
    const TraitItemReference &item_ref;
  };

  CandidateType type;
  TyTy::BaseType *ty;
  union Candidate
  {
    ImplItemCandidate impl;
    TraitItemCandidate trait;

    Candidate (ImplItemCandidate impl) : impl (impl) {}
    Candidate (TraitItemCandidate trait) : trait (trait) {}
  } item;

  PathProbeCandidate (CandidateType type, TyTy::BaseType *ty,
		      ImplItemCandidate impl)
    : type (type), ty (ty), item (impl)
  {}

  PathProbeCandidate (CandidateType type, TyTy::BaseType *ty,
		      TraitItemCandidate trait)
    : type (type), ty (ty), item (trait)
  {}

  std::string as_string () const
  {
    return "PathProbe candidate TODO - as_string";
  }

  bool is_impl_candidate () const
  {
    switch (type)
      {
      case IMPL_CONST:
      case IMPL_TYPE_ALIAS:
      case IMPL_FUNC:
	return true;

      default:
	return false;
      }
    gcc_unreachable ();
  }

  bool is_trait_candidate () const
  {
    switch (type)
      {
      case TRAIT_ITEM_CONST:
      case TRAIT_TYPE_ALIAS:
      case TRAIT_FUNC:
	return true;

      default:
	return false;
      }
    gcc_unreachable ();
  }
};

class PathProbeType : public TypeCheckBase
{
  using Rust::Resolver::TypeCheckBase::visit;

public:
  static std::vector<PathProbeCandidate>
  Probe (const TyTy::BaseType *receiver,
	 const HIR::PathIdentSegment &segment_name)
  {
    PathProbeType probe (receiver, segment_name);
    probe.process_impl_items_for_candidates ();
    probe.process_traits_for_candidates (TypeBoundsProbe::Probe (receiver));
    return probe.candidates;
  }

  void visit (HIR::TypeAlias &alias) override
  {
    Identifier name = alias.get_new_type_name ();
    if (search.as_string ().compare (name) == 0)
      {
	HirId tyid = alias.get_mappings ().get_hirid ();
	TyTy::BaseType *ty = nullptr;
	bool ok = context->lookup_type (tyid, &ty);
	rust_assert (ok);

	PathProbeCandidate::ImplItemCandidate impl_item_candidate{&alias,
								  current_impl};
	PathProbeCandidate candidate{
	  PathProbeCandidate::CandidateType::IMPL_TYPE_ALIAS, ty,
	  impl_item_candidate};
	candidates.push_back (std::move (candidate));
      }
  }

  void visit (HIR::ConstantItem &constant) override
  {
    Identifier name = constant.get_identifier ();
    if (search.as_string ().compare (name) == 0)
      {
	HirId tyid = constant.get_mappings ().get_hirid ();
	TyTy::BaseType *ty = nullptr;
	bool ok = context->lookup_type (tyid, &ty);
	rust_assert (ok);

	PathProbeCandidate::ImplItemCandidate impl_item_candidate{&constant,
								  current_impl};
	PathProbeCandidate candidate{
	  PathProbeCandidate::CandidateType::IMPL_CONST, ty,
	  impl_item_candidate};
	candidates.push_back (std::move (candidate));
      }
  }

  void visit (HIR::Function &function) override
  {
    Identifier name = function.get_function_name ();
    if (search.as_string ().compare (name) == 0)
      {
	HirId tyid = function.get_mappings ().get_hirid ();
	TyTy::BaseType *ty = nullptr;
	bool ok = context->lookup_type (tyid, &ty);
	rust_assert (ok);

	PathProbeCandidate::ImplItemCandidate impl_item_candidate{&function,
								  current_impl};
	PathProbeCandidate candidate{
	  PathProbeCandidate::CandidateType::IMPL_FUNC, ty,
	  impl_item_candidate};
	candidates.push_back (std::move (candidate));
      }
  }

private:
  void process_impl_items_for_candidates ()
  {
    mappings->iterate_impl_items ([&] (HirId id, HIR::ImplItem *item,
				       HIR::ImplBlock *impl) mutable -> bool {
      process_impl_item_candidate (id, item, impl);
      return true;
    });
  }

  void process_impl_item_candidate (HirId id, HIR::ImplItem *item,
				    HIR::ImplBlock *impl)
  {
    current_impl = impl;
    HirId impl_ty_id = impl->get_type ()->get_mappings ().get_hirid ();
    TyTy::BaseType *impl_block_ty = nullptr;
    bool ok = context->lookup_type (impl_ty_id, &impl_block_ty);
    rust_assert (ok);

    if (!receiver->can_eq (impl_block_ty, false))
      return;

    // lets visit the impl_item
    item->accept_vis (*this);
  }

  void process_traits_for_candidates (
    std::vector<std::reference_wrapper<TraitReference>> traits)
  {
    for (TraitReference &trait_ref : traits)
      {
	const TraitItemReference &trait_item_ref
	  = trait_ref.lookup_trait_item (search.as_string ());
	if (trait_item_ref.is_error ())
	  continue;

	// if we found a candidate it must be an optional one with an
	// implementation
	if (!trait_item_ref.is_optional ())
	  continue;

	PathProbeCandidate::CandidateType candidate_type;
	switch (trait_item_ref.get_trait_item_type ())
	  {
	  case TraitItemReference::TraitItemType::FN:
	    candidate_type = PathProbeCandidate::CandidateType::TRAIT_FUNC;
	    break;
	  case TraitItemReference::TraitItemType::CONST:
	    candidate_type
	      = PathProbeCandidate::CandidateType::TRAIT_ITEM_CONST;
	    break;
	  case TraitItemReference::TraitItemType::TYPE:
	    candidate_type
	      = PathProbeCandidate::CandidateType::TRAIT_TYPE_ALIAS;
	    break;

	  case TraitItemReference::TraitItemType::ERROR:
	    gcc_unreachable ();
	    break;
	  }

	PathProbeCandidate::TraitItemCandidate trait_item_candidate{
	  trait_ref, trait_item_ref};
	PathProbeCandidate candidate{candidate_type,
				     trait_item_ref.get_tyty (),
				     {trait_item_candidate}};
	candidates.push_back (std::move (candidate));
      }
  }

private:
  PathProbeType (const TyTy::BaseType *receiver,
		 const HIR::PathIdentSegment &query)
    : TypeCheckBase (), receiver (receiver), search (query),
      current_impl (nullptr)
  {}

  const TyTy::BaseType *receiver;
  const HIR::PathIdentSegment &search;
  std::vector<PathProbeCandidate> candidates;
  HIR::ImplBlock *current_impl;
};

class ReportMultipleCandidateError : private TypeCheckBase
{
  using Rust::Resolver::TypeCheckBase::visit;

public:
  static void Report (std::vector<PathProbeCandidate> &candidates,
		      const HIR::PathIdentSegment &query, Location query_locus)
  {
    RichLocation r (query_locus);
    ReportMultipleCandidateError visitor (r);
    for (auto &c : candidates)
      {
	switch (c.type)
	  {
	  case PathProbeCandidate::CandidateType::IMPL_CONST:
	  case PathProbeCandidate::CandidateType::IMPL_TYPE_ALIAS:
	  case PathProbeCandidate::CandidateType::IMPL_FUNC:
	    c.item.impl.impl_item->accept_vis (visitor);
	    break;

	  case PathProbeCandidate::CandidateType::TRAIT_ITEM_CONST:
	  case PathProbeCandidate::CandidateType::TRAIT_TYPE_ALIAS:
	  case PathProbeCandidate::CandidateType::TRAIT_FUNC:
	    r.add_range (c.item.trait.item_ref.get_locus ());
	    break;
	  }
      }

    rust_error_at (r, "multiple applicable items in scope for: %s",
		   query.as_string ().c_str ());
  }

  void visit (HIR::TypeAlias &alias) override
  {
    r.add_range (alias.get_locus ());
  }

  void visit (HIR::ConstantItem &constant) override
  {
    r.add_range (constant.get_locus ());
  }

  void visit (HIR::Function &function) override
  {
    r.add_range (function.get_locus ());
  }

private:
  ReportMultipleCandidateError (RichLocation &r) : TypeCheckBase (), r (r) {}

  RichLocation &r;
};

} // namespace Resolver
} // namespace Rust

#endif // RUST_HIR_PATH_PROBE_H
