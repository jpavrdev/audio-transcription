// Edge Function: cria um usuario novo. so admin pode chamar.
// usa a service key (injetada pelo supabase no ambiente) pra criar a conta no Auth.
// o gatilho ao_criar_usuario cria o perfil com o papel que vem nos metadados.
import { createClient } from 'https://esm.sh/@supabase/supabase-js@2'

const DOMINIO = 'mamede.local'
const cors = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
  'Access-Control-Allow-Methods': 'POST, OPTIONS',
}

function json(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { ...cors, 'Content-Type': 'application/json' },
  })
}

Deno.serve(async (req) => {
  if (req.method === 'OPTIONS') return new Response('ok', { headers: cors })
  try {
    if (req.method !== 'POST') return json({ erro: 'metodo invalido' })

    const url = Deno.env.get('SUPABASE_URL')!
    const anon = Deno.env.get('SUPABASE_ANON_KEY')!
    const service = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!

    // quem esta chamando? confere pela sessao no header Authorization
    const auth = req.headers.get('Authorization') ?? ''
    const comoUsuario = createClient(url, anon, { global: { headers: { Authorization: auth } } })
    const { data: { user } } = await comoUsuario.auth.getUser()
    if (!user) return json({ erro: 'nao autenticado' })

    // so admin ativo pode criar usuario
    const admin = createClient(url, service)
    const { data: perfil } = await admin.from('perfis').select('papel, ativo').eq('id', user.id).single()
    if (!perfil || !perfil.ativo || perfil.papel !== 'admin') {
      return json({ erro: 'apenas admin pode criar usuarios' })
    }

    // valida a entrada
    const body = await req.json().catch(() => ({}))
    const usuario = String(body?.usuario ?? '').trim().toLowerCase()
    const senha = String(body?.senha ?? '')
    const papel = String(body?.papel ?? '')
    if (!usuario) return json({ erro: 'informe o nome de usuario' })
    if (senha.length < 8) return json({ erro: 'senha muito curta (minimo 8 caracteres)' })
    if (!['funcionario', 'gerente', 'admin'].includes(papel)) return json({ erro: 'papel invalido' })

    // cria a conta no Auth ja confirmada; o gatilho cria o perfil com o papel
    const email = usuario.includes('@') ? usuario : `${usuario}@${DOMINIO}`
    const { data: novo, error } = await admin.auth.admin.createUser({
      email,
      password: senha,
      email_confirm: true,
      user_metadata: { papel },
    })
    if (error) {
      const msg = /already|exist|registered/i.test(error.message)
        ? 'ja existe um usuario com esse nome'
        : error.message
      return json({ erro: msg })
    }
    return json({ ok: true, id: novo.user?.id })
  } catch (e) {
    return json({ erro: String((e as Error)?.message ?? e) }, 500)
  }
})
