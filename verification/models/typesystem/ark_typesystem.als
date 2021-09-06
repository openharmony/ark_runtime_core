/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

module ark_typesystem

sig Sort {}

sig Type {}

enum Variance {
  Covariant,
  Contrvariant,
  Invariant
}

sig Param {
  variance: one Variance,
  type: one Type
} {
  // all params are different
  all disj p1, p2 : Param
  | p1.@variance != p2.@variance or
    p1.@type != p2.@type
}

// to avoid higher-order relations, using one level of
// indirection here, access to signatures via params atoms
sig Params {
  signature: seq Param
} {
  // all signatures are different for different params
  all disj p1, p2 : Params
  | p1.@signature != p2.@signature
}

// to help vizualization
pred subtype_params_for_vizualization [
  param_subtyping : Param -> Param,
  params_subtyping: Params -> Params,
  subtyping: Type -> Type
] {
  all p1,p2: Param
  | subtype[p1,p2,subtyping] iff
    p1 -> p2 in param_subtyping

  all ps1, ps2: Params
  | subtype[ps1,ps2, subtyping] iff
    ps1 -> ps2 in params_subtyping
}

// constraints for type universe
pred is_correct [universe: Sort -> Params -> Type] {
  // one type  per one pair sort->params
  all t: Type | one universe.t
  all disj t1,t2: Type | universe.t1 != universe.t2
  all t: Type | one universe.sort[t]
  all t: Type | one universe.params[t]

  // if sort has several params tuples of equal length,
  // then their sequencies of variances should match
  // (otherwise it is unclear how to calculate subtyping)
  all s: Sort
  | all ps1, ps2: s.universe.Type
  | #ps1 = #ps2 implies
    all idx: ps1.signature.inds
    | ps1.signature[idx].variance = ps1.signature[idx].variance

  // type cannot be present in its own parameters
  all t: Type| t not in universe.signature[t].elems.type
}

// p1 <: p2
// check if params are in subtyping relation
pred subtype [p1:Param, p2:Param, subtyping: Type -> Type] {
  let v1 = p1.variance,
       v2 = p2.variance
  {
    v1 = Invariant and v2 = Invariant implies
    p1.type -> p2.type in subtyping and
    p2.type -> p1.type in subtyping

    v1 = Covariant and v2 = Covariant implies
    p1.type -> p2.type in subtyping

    v1 = Contrvariant and v2 = Contrvariant implies
    p2.type -> p1.type in subtyping
  }
}

// sig1 <: sig2
// check signatures subtyping
pred subtype [sig1:Params, sig2:Params, subtyping: Type -> Type] {
  // two signatures are in subtyping relation if
  let sig1 = sig1.signature,
       sig2 = sig2.signature
  {
    #sig1 = #sig2 // they are of same length
    all idx : sig1.inds  // and all parameters in subtyping relation
    | subtype[sig1[idx], sig2[idx], subtyping]
  }
}

// aux functions for readbility
fun sort[universe: Sort->Params->Type, t: Type] : Sort { universe.t.Params }
fun params[universe: Sort->Params->Type, t: Type] : Params { universe.t[universe.sort[t]] }
fun signature[universe: Sort->Params->Type, t: Type] : seq Param { universe.params[t].signature }

pred non_parameterized[universe: Sort->Params->Type, t: Type] { universe.signature[t].isEmpty }

fun same_sort_arity_subtypeable[universe: Sort->Params->Type, subtyping: Type -> Type] : Type -> Type {
  {t1, t2 : Type |
    universe.sort[t1] = universe.sort[t2] and
    subtype[universe.params[t1], universe.params[t2], subtyping]
  }
}

fun all_subtypeable[universe: Sort->Params->Type, subtyping: Type -> Type] : Type -> Type {
  {t1, t2 : Type |
      universe.sort[t1] != universe.sort[t2] or
      #universe.signature[t1] != #universe.signature[t2] or
      // of same sort + arity and signatures in corresponding relation
      subtype[universe.params[t1], universe.params[t2], subtyping]
  }
}

pred is_correct[universe: Sort->Params->Type, subtyping: Type -> Type] {
  all t : Type | t ->t in subtyping
  ^subtyping in subtyping
  subtyping in all_subtypeable[universe, subtyping]
  same_sort_arity_subtypeable[universe, subtyping] in subtyping
}

// panda type system
private one sig TestTypeSystem {
  // each type - is parameterized sort
  universe: Sort -> Params -> Type,

  // subtyping relation
  subtyping: Type -> Type,

  // auxiliary, for better visual representation of
  // relations between params and signatures
  params_subtyping: Params -> Params,
  param_subtyping: Param -> Param
} {
  universe.is_correct
  subtype_params_for_vizualization [
    param_subtyping,
    params_subtyping,
    subtyping
  ]
}

// let's look at some instances of correct subtyping
show: run {
  #universe > 2
  #Params > 2
  #Sort > 1
  some disj t1,t2: Type | TestTypeSystem.universe.sort[t1] = TestTypeSystem.universe.sort[t2]
  TestTypeSystem.universe.is_correct[TestTypeSystem.subtyping]
}
