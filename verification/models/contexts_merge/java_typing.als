/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

module java_typing

abstract sig Type { subtypes: set Type }
one sig Top extends Type {} { no Top.~@subtypes }
one sig Bot extends Type {} { no Bot.@subtypes }

sig Class, Interface extends Type {}
one sig Object extends Type {}
one sig NullType extends Type {}

let ProperType = Type - Top - Bot - NullType

fact TypeHierarchy {
  ProperType in Object.*subtypes
  no t: ProperType | t in t.^subtypes
  all c: Class | lone c.~subtypes & Class
  all t: ProperType + NullType | t in Bot.~subtypes and t in Top.subtypes
  all i: Interface | no i.~subtypes & Class
  all i: ProperType | NullType in i.subtypes
  NullType not in NullType.subtypes
}

abstract sig Value {type: Class + NullType}

sig Instance extends Value {} { type in Class}
one sig Null extends Value {} { type = NullType}

sig Register {holds: one Value, type: Type}
fact Soundness { all r: Register | r.holds.type in r.type.*subtypes }
pred NonNull[r: Register] { r.type != NullType }
pred Valid[r: Register] { r.type in ProperType }
