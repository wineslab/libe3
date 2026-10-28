/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "libe3", "index.html", [
    [ "Latency recording (latrec)", "latrec_guide.html", [
      [ "Latency recording (latrec)", "latrec_guide.html#autotoc_md8", [
        [ "Why a ring, and why one gate", "latrec_guide.html#autotoc_md9", [
          [ "Matching the flag downstream", "latrec_guide.html#autotoc_md10", null ],
          [ "Python (libe3py)", "latrec_guide.html#autotoc_md11", null ],
          [ "Where the rings go", "latrec_guide.html#autotoc_md12", null ]
        ] ],
        [ "Clock model", "latrec_guide.html#autotoc_md13", null ],
        [ "Ring naming and sizing", "latrec_guide.html#autotoc_md14", null ],
        [ "Stage catalog", "latrec_guide.html#autotoc_md15", [
          [ "Joining across components", "latrec_guide.html#autotoc_md16", null ],
          [ "Attribution, and where the ring name stops being enough", "latrec_guide.html#autotoc_md17", [
            [ "Possible follow-up: tier-aware attribution", "latrec_guide.html#autotoc_md18", null ]
          ] ],
          [ "Granularity, and adding an identifier", "latrec_guide.html#autotoc_md19", null ]
        ] ],
        [ "Capture → CSV workflow", "latrec_guide.html#autotoc_md20", null ],
        [ "CI overhead gate", "latrec_guide.html#autotoc_md21", null ]
      ] ]
    ] ],
    [ "The E3-only end-to-end loop (Path A)", "path_a_e3_loop.html", [
      [ "Path A — the E3-only end-to-end loop", "path_a_e3_loop.html#autotoc_md22", [
        [ "Box order", "path_a_e3_loop.html#autotoc_md23", null ],
        [ "Forward leg: RAN to dApp (<span class=\"tt\">leg=ind_up</span>)", "path_a_e3_loop.html#autotoc_md24", [
          [ "Caveat on A6 and the wire", "path_a_e3_loop.html#autotoc_md25", null ]
        ] ],
        [ "Return leg: dApp to RAN (<span class=\"tt\">leg=ctrl_down</span> for control, <span class=\"tt\">report_up</span> for reports)", "path_a_e3_loop.html#autotoc_md26", [
          [ "Two distinct terminations", "path_a_e3_loop.html#autotoc_md27", null ]
        ] ],
        [ "Aggregates", "path_a_e3_loop.html#autotoc_md28", null ],
        [ "Deployment variants", "path_a_e3_loop.html#autotoc_md29", [
          [ "Comparing against a hand-rolled integration", "path_a_e3_loop.html#autotoc_md30", null ]
        ] ],
        [ "Not yet instrumented", "path_a_e3_loop.html#autotoc_md31", null ]
      ] ]
    ] ],
    [ "The full E2-E3 loop (Path B)", "path_b_e2_e3_loop.html", [
      [ "Path B — the full E2-E3 loop", "path_b_e2_e3_loop.html#autotoc_md32", [
        [ "Box order", "path_b_e2_e3_loop.html#autotoc_md33", null ],
        [ "The two RIC profiles", "path_b_e2_e3_loop.html#autotoc_md34", null ],
        [ "Report-up leg (<span class=\"tt\">leg=report_up</span>)", "path_b_e2_e3_loop.html#autotoc_md35", null ],
        [ "Policy-down leg (<span class=\"tt\">leg=policy_down</span>)", "path_b_e2_e3_loop.html#autotoc_md36", null ],
        [ "Mitigation leg (<span class=\"tt\">leg=mitigation</span>)", "path_b_e2_e3_loop.html#autotoc_md37", null ],
        [ "Aggregates", "path_b_e2_e3_loop.html#autotoc_md38", null ],
        [ "Joining: the <span class=\"tt\">sequenceId</span>", "path_b_e2_e3_loop.html#autotoc_md39", [
          [ "Where it shows up in the rings", "path_b_e2_e3_loop.html#autotoc_md40", null ]
        ] ],
        [ "Not yet instrumented", "path_b_e2_e3_loop.html#autotoc_md41", null ]
      ] ]
    ] ],
    [ "Topics", "topics.html", "topics" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"classlibe3_1_1E3Connector.html",
"classlibe3_1_1Logger.html#a8f40bcd6249ad424b8eb15e60049f65e",
"classlibe3_1_1py_1_1DAppSession.html#a1d7471730e97aa1230ce3cfe094078fe",
"latrec_guide.html#autotoc_md12",
"structlatrec__rec.html#a4650facda3586c172cf7779ae480e62b",
"structlibe3_1_1SubscriptionRequest.html#ae8ba1185c46760e64db97f510c8b4c0b"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
var LISTOFALLMEMBERS = 'List of all members';