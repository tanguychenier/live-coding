-- isaac-like, love2d, from scratch
-- the room, sam, walking, shooting, a first enemy
-- art from an old proto of mine (2019)

-- Measured on room.png: the interior is a 256x128 rectangle inside a black
-- outline, 8x4 tiles of 32 px. The walls take the rest.
ROOM_W, ROOM_H = 320, 192
PLAY = { x1 = 32, y1 = 32, x2 = 287, y2 = 159 }

-- 710x400 keeps the room at an exact scale of 2, so no pixel is stretched.
WIN_W, WIN_H = 710, 400
SCALE, OFF_X, OFF_Y = 2, 35, 8

PLAYER_SPEED  = 78
SHOT_SPEED    = 210
SHOT_COOLDOWN = 0.22
INVUL_TIME    = 1.2
KNOCKBACK     = 16     -- push applied to the monster that just hit you
ENTRY_GRACE   = 0.5    -- brief invulnerability when entering a room

-- room.png draws no doorway: the door sprites are what pierce the wall
DOOR_POS = {
  n = { x = 160, y = 20  },
  s = { x = 160, y = 172 },
  w = { x = 20,  y = 96  },
  e = { x = 300, y = 96  },
}

-- where the player lands when entering from a given side
ENTRY = {
  n = { x = 160,          y = PLAY.y2 - 12 },
  s = { x = 160,          y = PLAY.y1 + 12 },
  w = { x = PLAY.x2 - 12, y = 96 },
  e = { x = PLAY.x1 + 12, y = 96 },
}

ENEMIES = {
  mob1   = { art = "mob1",   w = 11, h = 8,  hp = 2, speed = 34, ai = "chase"  },
  mob2   = { art = "mob2",   w = 26, h = 10, hp = 4, speed = 22, ai = "chase"  },
  mob3   = { art = "mob3",   w = 14, h = 11, hp = 3, speed = 30, ai = "wander" },
  spider = { art = "spider", w = 15, h = 15, hp = 2, speed = 46, ai = "chase", fps = 10 },
skull  = { art = "skull",  w = 26, h = 26, hp = 4,  speed = 0,  ai = "turret", fire = 1.7 },
  boss   = { art = "mobBoss", w = 32, h = 20, hp = 20, speed = 46, ai = "boss" },
}
SPAWNABLE = { "mob1", "mob2", "mob3", "spider", "skull" }
MOBILES   = { "mob1", "mob2", "mob3", "spider" }   -- SPAWNABLE without the turret
CROSS     = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }

art = {}

function love.load()
  love.window.setMode(WIN_W, WIN_H, { vsync = 1, resizable = false })
  love.window.setTitle("Sam - TEC")
  -- must be set before any newImage, or every sprite comes out blurred
  love.graphics.setDefaultFilter("nearest", "nearest")

  local function img(name) return love.graphics.newImage("art/" .. name .. ".png") end

  art.room = img("room")
  art.bullet = img("bulletSam")
  art.mob1  = img("mob1")
  art.mob2  = img("mob2")
  art.mob3  = img("mob3")
  art.skull = img("skull")
  art.mobBoss = img("mobBoss")
  art.life  = img("life")
  art.doorClosed = img("door_closed")
  art.doorOpen   = img("door_open")
  art.mapRoom    = img("ui_map_room")
  art.mapBoss    = img("ui_map_bossroom")
  art.doorBoss   = img("door_boss")

  art.body, art.head = {}, {}

  sfx = {}
  local function son(name) return love.audio.newSource("son/" .. name .. ".wav", "static") end
  sfx.shot = son("tir")
  sfx.hit  = son("impact")
  sfx.dead = son("mort_ennemi")
  sfx.hurt = son("degat")
  sfx.door  = son("porte_ouverte")
  sfx.clear = son("salle_nettoyee")
  sfx.key    = son("cle")
  sfx.pickup = son("ramassage")
  sfx.boss = son("boss")
  sfx.lose = son("mort_joueur")
  for _, s in pairs(sfx) do s:setVolume(0.55) end
  for i = 1, 8 do art.body[i] = img("sam_" .. i)      end
  for i = 1, 4 do art.head[i] = img("sam_tete_" .. i) end
  art.spider = {}
  for i = 1, 4 do art.spider[i] = img("spider" .. i) end
  art.key = {}
  for i = 1, 5 do art.key[i] = img("keyRogue" .. i) end

  reset(os.time() % 100000)
end

function love.draw()
  love.graphics.push()
  love.graphics.translate(OFF_X, OFF_Y)
  love.graphics.scale(SCALE)

  love.graphics.draw(art.room, 0, 0)
  drawDoors()
  drawPlayer()
  drawBullets()
  drawEnemies()
  drawDrops()
  drawHud()
  drawBossBar()

  love.graphics.pop()

  -- screen coordinates, so the text is not blown up by the scale of 2
  love.graphics.setColor(1, 1, 1, 0.35)
  love.graphics.print("seed " .. g.seed, 6, love.graphics.getHeight() - 18)
  love.graphics.setColor(1, 1, 1)
  if g.message then
    love.graphics.printf(g.message, 0, love.graphics.getHeight() - 26,
                         love.graphics.getWidth(), "center")
  end
  if g.state == "dead" then
    drawBanner("Sam is dead", "R to replay this floor   -   N for a new floor")
  elseif g.state == "won" then
    drawBanner("Boss down", "N for a new floor")
  end
end

function love.keypressed(key)
  if key == "escape" then
    love.event.quit()
  elseif key == "r" then
    reset(g.seed)
  elseif key == "n" then
    reset(os.time() % 100000)
  end
end

function frameOf(images, fps, time)
  return images[math.floor(time * fps) % #images + 1]
end

function drawCentered(image, x, y, flip)
  love.graphics.draw(image, x, y, 0, flip and -1 or 1, 1,
                     image:getWidth() / 2, image:getHeight() / 2)
end

function drawPlayer()
  local p = g.player
  -- blink while invulnerable, so a hit is readable
  if p.invul > 0 and math.floor(p.invul * 12) % 2 == 0 then return end
  local body
  if not p.moving then
    body = art.body[1]
  elseif p.face == "n" or p.face == "s" then
    body = art.body[5 + math.floor(g.time * 10) % 4]
  else
    body = art.body[1 + math.floor(g.time * 10) % 4]
  end
  drawCentered(body, p.x, p.y + 5, p.flip)
  -- sam_tete_* is a blink, not four directions: it animates on its own
  drawCentered(frameOf(art.head, 3, g.time), p.x, p.y - 6, p.flip)
end

function reset(seed)
  local floor = Floor.generate{ seed = seed, rooms = 12 }
  floor.start.visited = true

  g = {
    seed = seed, floor = floor, room = floor.start,
time = 0, state = "play", message = nil, msgTime = 0,
    bullets = {},
    player = { x = 160, y = 96, w = 13, h = 13,
               hp = 4, maxhp = 4, invul = 0, hasKey = false,
               face = "s", flip = false, moving = false, cooldown = 0 },
  }
  fillRoom(floor.start, seed)
  say("Find the key, then the boss")
end

function love.update(dt)
  -- a huge dt (window dragged) would let everything pass through walls
  dt = math.min(dt, 1 / 30)
  g.time = g.time + dt
  if g.msgTime > 0 then
    g.msgTime = g.msgTime - dt
    if g.msgTime <= 0 then g.message = nil end
  end
  if g.state ~= "play" then return end
  updatePlayer(dt)
  updateBullets(dt)
  updateEnemies(dt)
  updateRoom()
end

function updatePlayer(dt)
  local p  = g.player
  local kb = love.keyboard
  local dx, dy = 0, 0
  if kb.isDown("q") or kb.isDown("a") then dx = dx - 1 end
  if kb.isDown("d")                   then dx = dx + 1 end
  if kb.isDown("z") or kb.isDown("w") then dy = dy - 1 end
  if kb.isDown("s")                   then dy = dy + 1 end

  p.moving = (dx ~= 0 or dy ~= 0)
  if p.moving then
    -- normalise, or the diagonal is 41 % faster than a straight line
    local len = math.sqrt(dx * dx + dy * dy)
    p.x = p.x + dx / len * PLAYER_SPEED * dt
    p.y = p.y + dy / len * PLAYER_SPEED * dt
    if dx ~= 0 then
      p.face, p.flip = (dx < 0) and "w" or "e", dx < 0
    else
      p.face = (dy < 0) and "n" or "s"
    end
  end

  local hw, hh = p.w / 2, p.h / 2
  shoot(dt)
  p.invul = math.max(0, p.invul - dt)
  p.x = math.max(PLAY.x1 + hw, math.min(PLAY.x2 - hw, p.x))
  p.y = math.max(PLAY.y1 + hh, math.min(PLAY.y2 - hh, p.y))

  tryDoors()
end

function play(name)
  local s = sfx[name]
  if s then s:stop(); s:play() end
end

function newBullet(x, y, dx, dy, friendly)
  return { x = x, y = y, dx = dx, dy = dy, w = 5, h = 5,
           friendly = friendly, life = 2.5 }
end

function shoot(dt)
  local p  = g.player
  local kb = love.keyboard
  p.cooldown = math.max(0, p.cooldown - dt)

  local sx, sy = 0, 0
  if kb.isDown("left")  then sx = -1 end
  if kb.isDown("right") then sx =  1 end
  if kb.isDown("up")    then sy = -1 end
  if kb.isDown("down")  then sy =  1 end
  if (sx == 0 and sy == 0) or p.cooldown > 0 then return end

  -- no diagonals: horizontal wins when both are held
  if sx ~= 0 then sy = 0 end
  g.bullets[#g.bullets + 1] = newBullet(p.x, p.y - 2, sx * SHOT_SPEED, sy * SHOT_SPEED, true)
  p.cooldown = SHOT_COOLDOWN
  play("shot")
end

function updateBullets(dt)
  for i = #g.bullets, 1, -1 do
    local b = g.bullets[i]
    b.x, b.y = b.x + b.dx * dt, b.y + b.dy * dt
    b.life = b.life - dt
    local dead = b.life <= 0
      or b.x < PLAY.x1 or b.x > PLAY.x2 or b.y < PLAY.y1 or b.y > PLAY.y2

    if not dead and b.friendly then
      for j = #g.room.enemies, 1, -1 do
        local e = g.room.enemies[j]
        if hits(b, e) then
          e.hp, e.flash, dead = e.hp - 1, 0.1, true
          if e.hp <= 0 then
table.remove(g.room.enemies, j)
            play("dead")
            if e.kind == "boss" then g.state = "won" end
          else
            play("hit")
          end
          break
        end
      end
    elseif not dead and hits(b, g.player) then
      hurtPlayer()
      dead = true
    end
    if dead then table.remove(g.bullets, i) end
  end
end

function drawBullets()
  for _, b in ipairs(g.bullets) do
    if not b.friendly then love.graphics.setColor(1, 0.55, 0.45) end
    drawCentered(art.bullet, b.x, b.y, false)
    love.graphics.setColor(1, 1, 1)
  end
end

function hits(a, b)
  return math.abs(a.x - b.x) * 2 < (a.w + b.w)
     and math.abs(a.y - b.y) * 2 < (a.h + b.h)
end

function newEnemy(kind, x, y)
  local def = ENEMIES[kind]
  return { kind = kind, def = def, x = x, y = y,
           w = def.w, h = def.h, hp = def.hp, maxhp = def.hp,
           cooldown = def.fire and (0.4 + (x % 7) / 10) or nil,
           dirx = 0, diry = 0, turn = 0, flash = 0,
           phase = 0.8, charging = false }
end

function updateEnemies(dt)
  local p = g.player
  for i = #g.room.enemies, 1, -1 do
    local e   = g.room.enemies[i]
    local def = e.def
    e.flash = math.max(0, e.flash - dt)

    if def.ai == "chase" then
      local dx, dy = p.x - e.x, p.y - e.y
      local len = math.max(1, math.sqrt(dx * dx + dy * dy))
      e.x = e.x + dx / len * def.speed * dt
      e.y = e.y + dy / len * def.speed * dt

elseif def.ai == "boss" then
      -- charge then rest; chasing without pause makes the fight unwinnable
      e.phase = e.phase - dt
      if e.phase <= 0 then
        e.charging = not e.charging
        if e.charging then
          e.phase = 1.1
        else
          e.phase = 1.0
          for _, v in ipairs(CROSS) do
            g.bullets[#g.bullets + 1] = newBullet(e.x, e.y, v[1] * 105, v[2] * 105, false)
          end
        end
      end
      if e.charging then
        local dx, dy = p.x - e.x, p.y - e.y
        local len = math.max(1, math.sqrt(dx * dx + dy * dy))
        e.x = e.x + dx / len * def.speed * dt
        e.y = e.y + dy / len * def.speed * dt
      end

    elseif def.ai == "wander" then
      e.turn = e.turn - dt
      if e.turn <= 0 then
        local a = love.math.random() * math.pi * 2
        e.dirx, e.diry = math.cos(a), math.sin(a)
        e.turn = 0.8 + love.math.random() * 0.8
      end
      e.x = e.x + e.dirx * def.speed * dt
      e.y = e.y + e.diry * def.speed * dt
    end

    if def.fire then
      e.cooldown = e.cooldown - dt
      if e.cooldown <= 0 then
        e.cooldown = def.fire
        fireAt(e, p.x, p.y, 95)
      end
    end

    local hw, hh = e.w / 2, e.h / 2
    local cx = math.max(PLAY.x1 + hw, math.min(PLAY.x2 - hw, e.x))
    local cy = math.max(PLAY.y1 + hh, math.min(PLAY.y2 - hh, e.y))
    -- bounce the wanderer off the walls instead of pinning it there
    if cx ~= e.x then e.dirx = -e.dirx end
    if cy ~= e.y then e.diry = -e.diry end
    e.x, e.y = cx, cy

    if hits(e, p) then hurtPlayer(e) end
  end
end

function drawEnemies()
  for _, e in ipairs(g.room.enemies) do
    local a = art[e.def.art]
    local image = e.def.fps and frameOf(a, e.def.fps, g.time) or a
    if e.flash > 0 then love.graphics.setColor(1, 0.5, 0.5) end
    drawCentered(image, e.x, e.y, false)
    love.graphics.setColor(1, 1, 1)
  end
end

function hurtPlayer(source)
  local p = g.player
  if p.invul > 0 then return end
  p.hp    = p.hp - 1
  p.invul = INVUL_TIME
  play("hurt")

  -- without the push a chaser stays glued and drains a heart per second
  if source then
    local dx, dy = source.x - p.x, source.y - p.y
    local len = math.max(1, math.sqrt(dx * dx + dy * dy))
    source.x = source.x + dx / len * KNOCKBACK
    source.y = source.y + dy / len * KNOCKBACK
  end

  if p.hp <= 0 then
    g.state = "dead"
    play("lose")
  end
end

function drawHud()
  for i = 1, g.player.maxhp do
    local x = 6 + (i - 1) * 15
    love.graphics.setColor(1, 1, 1, i <= g.player.hp and 1 or 0.22)
    love.graphics.draw(art.life, x, 3)
  end
  love.graphics.setColor(1, 1, 1)
  if g.player.hasKey then
    love.graphics.draw(frameOf(art.key, 6, g.time), 6 + g.player.maxhp * 15, 1)
  end

  -- laid out on the floor extent, not the whole grid, or it spills over
  local cw, ch = 14, 9
  local ox = ROOM_W - 6 - (g.floor.maxx - g.floor.minx + 1) * cw
  for _, room in ipairs(g.floor.list) do
    if room.visited then
      local x = ox + (room.x - g.floor.minx) * cw
      local y = 4  + (room.y - g.floor.miny) * ch
      local image = (room.kind == "boss") and art.mapBoss or art.mapRoom
      local alpha = (room == g.room) and 1 or 0.45
      if room.kind == "key" and room.keyDrop then
        -- pulses while the key is still on the ground; the only hint there is
        love.graphics.setColor(1, 0.85, 0.3, 0.55 + 0.45 * math.abs(math.sin(g.time * 3)))
      elseif room.kind == "key" then
        love.graphics.setColor(1, 0.85, 0.3, alpha)
      else
        love.graphics.setColor(1, 1, 1, alpha)
      end
      love.graphics.draw(image, x, y)
    end
  end
  love.graphics.setColor(1, 1, 1)
end

-- --------------------------------------------------------------- the floor

Floor = {}
Floor.DIRS = {
  { name = "n", dx =  0, dy = -1, opposite = "s" },
  { name = "s", dx =  0, dy =  1, opposite = "n" },
  { name = "w", dx = -1, dy =  0, opposite = "e" },
  { name = "e", dx =  1, dy =  0, opposite = "w" },
}

-- explicit seed rather than math.random, so a floor can be replayed
function Floor.newRandom(seed)
  local state = math.floor(seed) % 2147483647
  if state <= 0 then state = state + 2147483646 end
  return function(n)
    state = (state * 16807) % 2147483647
    if n then return (state % n) + 1 end
    return state / 2147483647
  end
end

function Floor.at(floor, x, y)
  local row = floor.cells[y]
  return row and row[x]
end

function Floor.generate(opts)
  opts = opts or {}
  local w, h  = opts.w or 9, opts.h or 7
  local target = opts.rooms or 12
  local random = Floor.newRandom(opts.seed or 1)

  local cells, list = {}, {}
  for y = 1, h do cells[y] = {} end

  local function add(x, y, kind)
    local room = { x = x, y = y, kind = kind, doors = {}, cleared = false }
    cells[y][x] = room
    list[#list + 1] = room
    return room
  end

  local function neighbourCount(x, y)
    local n = 0
    for _, d in ipairs(Floor.DIRS) do
      local row = cells[y + d.dy]
      if row and row[x + d.dx] then n = n + 1 end
    end
    return n
  end

  local sx, sy = math.floor((w + 1) / 2), math.floor((h + 1) / 2)
  add(sx, sy, "start")

  -- rejecting cells that already touch two rooms is what gives branches, and
  -- therefore dead ends to put the boss and the key in
  while #list < target do
    local candidates = {}
    for _, room in ipairs(list) do
      for _, d in ipairs(Floor.DIRS) do
        local nx, ny = room.x + d.dx, room.y + d.dy
        if nx >= 1 and nx <= w and ny >= 1 and ny <= h
          and not cells[ny][nx] and neighbourCount(nx, ny) <= 1
        then
          candidates[#candidates + 1] = { x = nx, y = ny }
        end
      end
    end
    if #candidates == 0 then break end

    local placed = 0
    for _, c in ipairs(candidates) do
      if #list >= target then break end
      if not cells[c.y][c.x] and neighbourCount(c.x, c.y) <= 1 and random() < 0.55 then
        add(c.x, c.y, "normal")
        placed = placed + 1
      end
    end
    -- force one, or an unlucky wave leaves a two-room floor
    if placed == 0 then
      local c = candidates[random(#candidates)]
      add(c.x, c.y, "normal")
    end
  end

  for _, room in ipairs(list) do
    for _, d in ipairs(Floor.DIRS) do
      local row = cells[room.y + d.dy]
      room.doors[d.name] = (row and row[room.x + d.dx]) ~= nil
    end
  end

  -- breadth first from the start, to know how far each room is
  local start = cells[sy][sx]
  start.dist = 0
  local queue, head = { start }, 1
  while head <= #queue do
    local room = queue[head]
    head = head + 1
    for _, d in ipairs(Floor.DIRS) do
      local row = cells[room.y + d.dy]
      local nb  = row and row[room.x + d.dx]
      if nb and not nb.dist then
        nb.dist = room.dist + 1
        queue[#queue + 1] = nb
      end
    end
  end

  local function doorCount(room)
    local n = 0
    for _, open in pairs(room.doors) do
      if open then n = n + 1 end
    end
    return n
  end

  -- farthest first; the y/x tie breakers keep the ordering deterministic
  local deadEnds = {}
  for _, room in ipairs(list) do
    if room.kind == "normal" and doorCount(room) == 1 then
      deadEnds[#deadEnds + 1] = room
    end
  end
  table.sort(deadEnds, function(a, b)
    if a.dist ~= b.dist then return a.dist > b.dist end
    if a.y    ~= b.y    then return a.y < b.y end
    return a.x < b.x
  end)

  local function farthestNormal()
    local best
    for _, room in ipairs(list) do
      if room.kind == "normal" then
        if not best or room.dist > best.dist then best = room end
      end
    end
    return best
  end

  local boss = deadEnds[1] or farthestNormal()
  if boss then boss.kind = "boss" end
  local key = deadEnds[2] or farthestNormal()
  if key then key.kind = "key" end

  local minx, maxx, miny, maxy = w, 1, h, 1
  for _, room in ipairs(list) do
    if room.x < minx then minx = room.x end
    if room.x > maxx then maxx = room.x end
    if room.y < miny then miny = room.y end
    if room.y > maxy then maxy = room.y end
  end

  return { w = w, h = h, seed = opts.seed, cells = cells, list = list,
           start = start, boss = boss, key = key,
           minx = minx, maxx = maxx, miny = miny, maxy = maxy }
end

-- ---------------------------------------------------------------- the rooms

-- seeded per room, so a room you come back to holds the monsters you left
function fillRoom(room, seed)
  if room.spawned then return end
  room.spawned = true
  room.enemies = {}
  if room.kind == "start" then
    room.cleared = true
    return
  end
local rnd = Floor.newRandom(seed * 7919 + room.y * 131 + room.x)

  if room.kind == "boss" then
    room.enemies[1] = newEnemy("boss", 160, 64)
    return
  end

  local count = 1 + rnd(1 + math.min(2, math.floor(room.dist / 2)))
  local turret = false
  for _ = 1, count do
    local kind = SPAWNABLE[rnd(#SPAWNABLE)]
    -- two immobile shooters make a room harder than the boss; cap at one
    if kind == "skull" then
      if turret then kind = MOBILES[rnd(#MOBILES)] else turret = true end
    end
    -- never on a doorway, or you get hit the instant you walk in
    local x, y
    for attempt = 1, 8 do
      x = PLAY.x1 + 32 + rnd(PLAY.x2 - PLAY.x1 - 64)
      y = PLAY.y1 + 28 + rnd(PLAY.y2 - PLAY.y1 - 56)
      local tooClose = false
      for _, e in pairs(ENTRY) do
        local dx, dy = x - e.x, y - e.y
        if dx * dx + dy * dy < 46 * 46 then tooClose = true end
      end
      if not tooClose or attempt == 8 then break end
    end
    room.enemies[#room.enemies + 1] = newEnemy(kind, x, y)
  end
end

function changeRoom(dir)
  local nb = Floor.at(g.floor, g.room.x + dir.dx, g.room.y + dir.dy)
  if not nb then return end
  if nb.kind == "boss" and not g.player.hasKey then
    say("The boss door is locked - find the key")
    return
  end
  g.room = nb
  nb.visited = true
  fillRoom(nb, g.seed)
  g.bullets = {}
  play("door")
  if nb.kind == "boss" then play("boss") end

  local entry = ENTRY[dir.name]
  g.player.x, g.player.y = entry.x, entry.y
  g.player.invul = math.max(g.player.invul, ENTRY_GRACE)
end

function tryDoors()
  if not g.room.cleared then return end
  local p = g.player
  local hw, hh = p.w / 2, p.h / 2
  for _, d in ipairs(Floor.DIRS) do
    if g.room.doors[d.name] then
      local touching =
           (d.name == "n" and p.y <= PLAY.y1 + hh + 0.5 and math.abs(p.x - 160) < 18)
        or (d.name == "s" and p.y >= PLAY.y2 - hh - 0.5 and math.abs(p.x - 160) < 18)
        or (d.name == "w" and p.x <= PLAY.x1 + hw + 0.5 and math.abs(p.y - 96)  < 18)
        or (d.name == "e" and p.x >= PLAY.x2 - hw - 0.5 and math.abs(p.y - 96)  < 18)
      if touching then
        changeRoom(d)
        return
      end
    end
  end
end

function updateRoom()
  if not g.room.cleared and #g.room.enemies == 0 then
    g.room.cleared = true
    play("clear")
    if g.room.kind == "key" then
      g.room.keyDrop = { x = 160, y = 96, w = 20, h = 20 }
    end
  end

  local drop = g.room.keyDrop
  if drop and hits(drop, g.player) then
    g.room.keyDrop  = nil
    g.player.hasKey = true
    play("key")
    say("Key taken - the boss room is open")
  end
end

function drawDoors()
  for _, d in ipairs(Floor.DIRS) do
    if g.room.doors[d.name] then
      local pos = DOOR_POS[d.name]
      local nb  = Floor.at(g.floor, g.room.x + d.dx, g.room.y + d.dy)
      local image
      if nb and nb.kind == "boss" then
        image = art.doorBoss
      else
        image = g.room.cleared and art.doorOpen or art.doorClosed
      end
      drawCentered(image, pos.x, pos.y, false)
    end
  end
end

function fireAt(e, tx, ty, speed)
  local dx, dy = tx - e.x, ty - e.y
  local len = math.max(1, math.sqrt(dx * dx + dy * dy))
  g.bullets[#g.bullets + 1] = newBullet(e.x, e.y, dx / len * speed, dy / len * speed, false)
end

function say(text)
  g.message, g.msgTime = text, 1.8
end

function drawDrops()
  if g.room.keyDrop then
    drawCentered(frameOf(art.key, 6, g.time), g.room.keyDrop.x, g.room.keyDrop.y, false)
  end
end



function drawBossBar()
  local boss = g.room.enemies[1]
  if g.room.kind ~= "boss" or not boss or boss.kind ~= "boss" then return end
  local bw = PLAY.x2 - PLAY.x1
  love.graphics.setColor(0, 0, 0, 0.65)
  love.graphics.rectangle("fill", PLAY.x1, ROOM_H - 6, bw, 4)
  love.graphics.setColor(0.85, 0.2, 0.25)
  love.graphics.rectangle("fill", PLAY.x1, ROOM_H - 6, bw * boss.hp / boss.maxhp, 4)
  love.graphics.setColor(1, 1, 1)
end

function drawBanner(title, subtitle)
  local w, h = love.graphics.getDimensions()
  love.graphics.setColor(0, 0, 0, 0.72)
  love.graphics.rectangle("fill", 0, h / 2 - 46, w, 92)
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf(title,    0, h / 2 - 32, w, "center")
  love.graphics.printf(subtitle, 0, h / 2 + 2,  w, "center")
end