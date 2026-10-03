# Shared instruction emission and matching

Each backend keeps its internal emitter in `src/dxp/sm5/step/common/Emit.hpp/.cpp`
or `src/dxp/sm6/step/common/Emit.hpp/.cpp`, under the corresponding
`dxp::sm5::step::common` or `dxp::sm6::step::common` namespace. Consuming steps
import the names they use explicitly:

```cpp
namespace dxp::sm6::step {
using common::EmitContext;
using common::EmitInstruction;
// Step execution supplies bindings and the insertion builder.
}
```

Public recipe types retain their existing namespaces and headers. The emit
modules are internal; they do not add a public API or change the YAML surface.

SM5's `EmitContext` supplies captures, environment variables, register bindings,
registered templates, the template pool base, declaration metadata, and logging.
Optional local operand bindings take precedence over cross-step captures for
ordinary rewrite emits. Template bodies use their existing global capture scope
and temporary bindings. Repeat expansion restores shadowed variables after both
successful emission and errors. Instruction construction, operand resolution,
template/repeat expansion, blob copying, and extended-opcode generation belong
to `Emit.cpp`.

SM6's `EmitContext` supplies an LLVM insertion builder, module/DXIL state, local
values, environment values, optional global captures, and resource/function
resolvers. `apply_rule` supplies shader captures; `declare_function` supplies
parameters and earlier body results, with resource handles created inside the
helper. `EmitResult` tracks newly generated instructions separately from the
optional SSA value, so stores and void calls are successful emissions.

Neither emitter receives a `MatchResult`. Matching, capture publication,
insertion/replacement placement, and shader mutation stay in `ApplyRuleStep`.
SM6 also keeps dominance and live-use checks there. SM5 template declarations
register bodies for later expansion; SM6 function declarations execute the
emitter directly while constructing a helper.

The backends share this boundary and naming convention, while their contexts
reflect their different instruction models: SM5 register operands and token
streams, and SM6 typed SSA values and LLVM functions.

Matching follows the same organization in each backend's internal
`step/common/Match.hpp/.cpp`, under `step::common`. `ApplyRuleStep.cpp` explicitly
imports the match types and entry points it uses. Public pattern types remain
in `Rule.hpp`, with their existing namespaces and inclusion paths.

`MatchResult` contains matched instruction locations and local captures.
SM5's read-only `MatchContext` supplies existing captures and declaration
metadata; SM6's supplies environment values, existing captures, and DXIL
resource metadata, plus an error output for constant resolution. Matching
does not publish captures or mutate the shader. Sequence and window discovery
belong to `Match.cpp`; match selection, exports, capture publication, anchor
selection, and replacement checks stay in `ApplyRuleStep.cpp`. SM6's rewrite
operations are free helpers there rather than methods on `MatchResult`.
