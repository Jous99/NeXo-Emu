// SPDX-FileCopyrightText: Copyright 2026 NexoEmu Project
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Backend BCAT (Broadcast Content Access) para Raptor Network.
// BCAT es el servicio de Nintendo Switch que distribuye contenido
// de juegos (actualizaciones, eventos, noticias) via internet.
//
// En RaptorCitrus, el BCAT se redirige a los servidores de Raptor Network
// en vez de los de Nintendo, para evitar baneos y proveer contenido propio.

#pragma once

#include <string>

namespace Service::BCAT {

/// Hostname del servidor BCAT de Raptor Network.
/// Todo el contenido BCAT se descarga de aqui en vez de los servidores de Nintendo.
constexpr const char RAPTOR_BCAT_HOSTNAME[] = "bcat-lp1.raptor.network";

/// Tipo de cliente que se envia en el header de las peticiones BCAT.
/// Esto identifica al emulador ante el servidor.
constexpr const char RAPTOR_BCAT_CLIENT_TYPE[] = "NexoEmu";

} // namespace Service::BCAT
