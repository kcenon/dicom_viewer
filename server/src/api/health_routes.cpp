// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "health_routes.hpp"

#include "services/render/gpu_memory_budget_manager.hpp"

#include <nlohmann/json.hpp>

namespace dicom_viewer::server {

using routes::addCorsHeaders;
using nlohmann::json;

void registerHealthRoutes(routes::App* app,
                          services::GpuMemoryBudgetManager* gpuBudget,
                          const std::string& corsOrigin) {
    // GET /api/v1/health/gpu — GPU memory budget metrics
    CROW_ROUTE((*app), "/api/v1/health/gpu")(
        [corsOrigin, gpuBudget](const crow::request& /*req*/, crow::response& res) {
            addCorsHeaders(res, corsOrigin);

            json resp;

            if (gpuBudget) {
                auto m = gpuBudget->metrics();
                resp["available"]      = m.available;
                resp["gpuName"]        = m.gpuName;
                resp["totalMb"]        = m.totalBytes / (1024 * 1024);
                resp["usedMb"]         = m.usedBytes / (1024 * 1024);
                resp["freeMb"]         = m.freeBytes / (1024 * 1024);
                resp["utilization"]    = m.utilizationPercent;
                resp["activeSessions"] = m.activeSessionCount;
            } else {
                resp["available"]      = false;
                resp["gpuName"]        = "";
                resp["totalMb"]        = 0;
                resp["usedMb"]         = 0;
                resp["freeMb"]         = 0;
                resp["utilization"]    = 0.0;
                resp["activeSessions"] = 0;
            }

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });
}

} // namespace dicom_viewer::server
