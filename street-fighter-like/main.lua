-- hello everyone
-- today: a street fighter-like in love2d, from an empty file
-- four fighters, two stages, three rounds

W, H, SCALE = 272, 160, 4
ZOOM   = 2
GROUND = 142
ROUND_TIME  = 60
WINS_NEEDED = 2
GRAVITY    = 620
JUMP_SPEED = 210

FIGHTERS = {
  { id = "samurai", name = "SAMURAI", reach = 34, light = 5, heavy = 11, speed = 62 },
  { id = "ninja",   name = "NINJA",   reach = 28, light = 4, heavy = 9,  speed = 78 },
  { id = "ranger",   name = "RANGER",   reach = 30, light = 4, heavy = 10, speed = 58, arrows = true },
  { id = "valkyrie", name = "VALKYRIE", reach = 38, light = 6, heavy = 13, speed = 54 },
}

STAGES = {
  { id = "forest", name = "FOREST", layers = { "back", "middle", "lights", "front" } },
  { id = "city",   name = "CITY",   layers = { "back", "middle", "front", "ground" } },
}

-- everyone has the same animations, but not the same number of frames:
-- the samurai swings eight times where the ninja stabs in three
ANIMS = {
  idle   = { speed = 6,  loop = true },
  run    = { speed = 12, loop = true },
  jump   = { speed = 8,  loop = false },
  attack = { speed = 14, loop = false },
  hurt   = { speed = 12, loop = false },
}

function love.load()
  love.graphics.setDefaultFilter("nearest", "nearest")
  love.window.setMode(W * SCALE, H * SCALE, { vsync = 1 })
  love.window.setTitle("fight")

  art = {}
  for _, fighter in ipairs(FIGHTERS) do
    art[fighter.id] = {}
    for name in pairs(ANIMS) do
      art[fighter.id][name] = {}
      local i = 1
      while love.filesystem.getInfo("art/" .. fighter.id .. "/" .. name .. "_" .. i .. ".png") do
        art[fighter.id][name][i] = love.graphics.newImage("art/" .. fighter.id .. "/" .. name .. "_" .. i .. ".png")
        i = i + 1
      end
    end
  end

  for _, s in ipairs(STAGES) do
    art[s.id] = {}
    for _, layer in ipairs(s.layers) do
      art[s.id][layer] = love.graphics.newImage("art/" .. s.id .. "/" .. layer .. ".png")
    end
  end
  stage = STAGES[1]
  hitstop = 0

  music = {}
  for _, name in ipairs({ "menu", "fight1", "fight2", "victory" }) do
    music[name] = love.audio.newSource("musique/" .. name .. ".ogg", "stream")
    music[name]:setLooping(name ~= "victory")
    music[name]:setVolume(0.5)
  end

  sfx = {}
  for _, name in ipairs({ "vent", "touche", "touche_fort", "garde", "saut",
                          "atterrissage", "fleche", "ko", "cloche", "seconde",
                          "curseur", "choix", "victoire" }) do
    sfx[name] = love.audio.newSource("son/" .. name .. ".wav", "static")
  end

  fighters = {
    newFighter(FIGHTERS[1], 90, 1, { left = "q", right = "d", up = "z", down = "s" }),
    newFighter(FIGHTERS[2], W - 90, -1),
  }
  fighters[2].brain = true
  startRound()
  screen, clock = "title", 0
  cursor = { p1 = 1, p2 = 2, stage = 1 }
  demo, idle = false, 0
  playMusic("menu")
end

function love.draw()
  love.graphics.scale(SCALE, SCALE)
  if screen == "title" then
    drawTitle()
    return
  end
  if screen == "select" then
    drawSelect()
    return
  end
  if screen == "result" then
    drawStage()
    drawFloor()
    for _, fighter in ipairs(fighters) do drawFighter(fighter) end
    drawResult()
    return
  end
  drawStage()
  drawFloor()
  for _, fighter in ipairs(fighters) do drawFighter(fighter) end
  drawArrows()
  drawHud()
  if game.banner then drawBanner(game.banner) end
end

function newFighter(def, x, facing, controls)
  return {
    def = def, x = x, y = GROUND, vy = 0, facing = facing,
    controls = controls,
    hp = 100, shown = 100, state = "idle", frame = 1, timer = 0,
    brain = nil, think = 0, plan = "wait", hitDone = false,
    blocking = false, stun = 0, wins = 0, flash = 0,
  }
end

function setState(fighter, state)
  if fighter.state == state then return end
  fighter.state, fighter.frame, fighter.timer, fighter.hitDone = state, 1, 0, false
end

function frames(fighter)
  return art[fighter.def.id][fighter.state] or art[fighter.def.id].idle
end

function animate(fighter, dt)
  local anim = ANIMS[fighter.state] or ANIMS.idle
  fighter.timer = fighter.timer + dt * anim.speed
  while fighter.timer >= 1 do
    fighter.timer = fighter.timer - 1
    if fighter.frame < #frames(fighter) then
      fighter.frame = fighter.frame + 1
    elseif anim.loop then
      fighter.frame = 1
    end
  end
end

function lastFrame(fighter)
  return fighter.frame >= #frames(fighter)
end

function drawFighter(fighter)
  if fighter.flash > 0 then love.graphics.setColor(1, 0.55, 0.55) end
  if fighter.blocking then love.graphics.setColor(0.62, 0.8, 1) end
  drawFloorShadow(fighter)
  local list = frames(fighter)
  local image = list[math.min(fighter.frame, #list)]
  -- the sprite has four empty pixels under the feet: without this the
  -- fighter floats above the floor
  love.graphics.draw(image, fighter.x, fighter.y - 25 * ZOOM, 0, fighter.facing * ZOOM, ZOOM,
                     image:getWidth() / 2, 0)
  love.graphics.setColor(1, 1, 1)
end

function love.update(dt)
  clock = clock + dt
  if screen == "title" then
    idle = idle + dt
    if idle > 25 then startDemo() end
    return
  end
  if screen == "result" then
    for _, fighter in ipairs(fighters) do animate(fighter, dt) end
    if demo then
      idle = idle + dt
      if idle > 8 then startDemo() end
    end
    return
  end
  if screen ~= "fight" then return end

  if game.freeze > -1 then
    game.freeze = game.freeze - dt
    if game.freeze <= 0 and game.banner then game.banner = "FIGHT" end
    if game.freeze <= -0.8 then game.banner = nil end
    if game.freeze > 0 then return end
  end

  if game.over then
    game.over = game.over - dt
    for _, fighter in ipairs(fighters) do animate(fighter, dt) end
    if game.over <= 0 then
      if game.winner then
        game.banner = game.winner.def.name .. " WINS"
        play("victoire")
        playMusic("victory")
        screen = "result"
      else
        game.round = game.round + 1
        startRound()
      end
    end
    return
  end

  local before = math.ceil(game.clock)
  game.clock = math.max(0, game.clock - dt)
  if math.ceil(game.clock) ~= before and game.clock <= 10 then play("seconde") end
  if game.clock <= 0 then
    timeUp()
    return
  end

  if hitstop > 0 then
    hitstop = hitstop - dt
    return
  end

  updateFighter(fighters[1], fighters[2], dt)
  updateFighter(fighters[2], fighters[1], dt)
  separate(fighters[1], fighters[2])
  updateArrows(dt)
end

function readInput(fighter)
  local keys = fighter.controls
  if not keys then return { left = false, right = false, up = false, down = false } end
  return {
    left  = love.keyboard.isDown(keys.left),
    right = love.keyboard.isDown(keys.right),
    up    = love.keyboard.isDown(keys.up),
    down  = love.keyboard.isDown(keys.down),
  }
end

function walk(fighter, dir, dt)
  fighter.x = math.max(22, math.min(W - 22, fighter.x + dir * fighter.def.speed * dt))
end

function updateFighter(fighter, other, dt)
  fighter.flash = math.max(0, fighter.flash - dt)
  fighter.shown = fighter.shown + (fighter.hp - fighter.shown) * math.min(1, dt * 4)

  fighter.facing = (other.x < fighter.x) and -1 or 1
  local input = fighter.brain and brainInput(fighter, other, dt) or readInput(fighter)
  fighter.blocking = false
  local back = (fighter.facing == 1) and input.left or input.right
  if back and not input.up and grounded(fighter) then
    fighter.blocking = true
    setState(fighter, "idle")
    animate(fighter, dt)
    return
  end

  local busy = fighter.state == "attack" or fighter.state == "hurt"
  if busy then
    -- a swing cannot be cancelled by walking: without this the state machine
    -- starts an attack and the very next frame turns it back into idle
  elseif input.left or input.right then
    walk(fighter, input.left and -1 or 1, dt)
    setState(fighter, "run")
  else
    setState(fighter, "idle")
  end
  if fighter.state == "attack" then
    resolveAttack(fighter, other)
    if lastFrame(fighter) and fighter.timer >= 0.6 then setState(fighter, "idle") end
    animate(fighter, dt)
    return
  end
  if fighter.state == "hurt" then
    if lastFrame(fighter) and fighter.timer > 0.4 then setState(fighter, "idle") end
    animate(fighter, dt)
    return
  end

  if input.up then jump(fighter) end
  if not grounded(fighter) or fighter.vy < 0 then
    fighter.vy = fighter.vy + GRAVITY * dt
    fighter.y = fighter.y + fighter.vy * dt
    if fighter.y >= GROUND then
      fighter.y, fighter.vy = GROUND, 0
      play("atterrissage")
      setState(fighter, "idle")
    end
  end

  animate(fighter, dt)
end

function play(name)
  if sfx[name] then
    sfx[name]:stop()
    sfx[name]:play()
  end
end

function grounded(fighter)
  return fighter.y >= GROUND - 0.5
end

function jump(fighter)
  if grounded(fighter) then
    fighter.vy = -JUMP_SPEED
    setState(fighter, "jump")
    play("saut")
  end
end

function drawStage()
  for i, layer in ipairs(stage.layers) do
    local image = art[stage.id][layer]
    love.graphics.draw(image, 0, H - image:getHeight())
  end
end

function drawFloor()
  love.graphics.setColor(0.06, 0.07, 0.09)
  love.graphics.rectangle("fill", 0, GROUND, W, H - GROUND)
  love.graphics.setColor(0.16, 0.18, 0.22)
  love.graphics.rectangle("fill", 0, GROUND, W, 2)
  love.graphics.setColor(1, 1, 1)
end

function separate(a, b)
  local gap = math.abs(a.x - b.x)
  if gap < 26 then
    local push = (26 - gap) / 2
    local dir = (a.x < b.x) and -1 or 1
    a.x = math.max(22, math.min(W - 22, a.x + dir * push))
    b.x = math.max(22, math.min(W - 22, b.x - dir * push))
  end
end

function strike(fighter, heavy)
  if fighter.state == "attack" or fighter.state == "hurt" or not grounded(fighter) then return end
  setState(fighter, "attack")
  fighter.heavy = heavy
  play("vent")
end

function hitbox(fighter)
  return { x = fighter.x + fighter.facing * 10, y = fighter.y - 46, w = fighter.def.reach, h = 34 }
end

function body(fighter)
  return { x = fighter.x - 13, y = fighter.y - 52, w = 26, h = 52 }
end

function overlap(a, b)
  local ax = math.min(a.x, a.x + a.w)
  local bx = math.min(b.x, b.x + b.w)
  return ax < bx + math.abs(b.w) and bx < ax + math.abs(a.w)
     and a.y < b.y + b.h and b.y < a.y + a.h
end

function resolveAttack(fighter, other)
  -- the blow only lands in the middle of the swing, whatever its length
  local step = fighter.frame / #art[fighter.def.id].attack
  if fighter.hitDone or step < 0.4 or step > 0.85 then return end
  if fighter.def.arrows and not fighter.heavy then
    fighter.hitDone = true
    arrows[#arrows + 1] = newArrow(fighter)
    play("fleche")
    return
  end

  local zone = hitbox(fighter)
  zone.x = math.min(zone.x, zone.x + fighter.facing * zone.w)
  if not overlap(zone, body(other)) then return end
  fighter.hitDone = true
  local damage = fighter.heavy and fighter.def.heavy or fighter.def.light
  if other.blocking then
    hurt(other, math.max(1, math.floor(damage / 4)), true)
  else
    hurt(other, damage, false)
    other.x = math.max(22, math.min(W - 22, other.x + fighter.facing * 6))
  end
end

function hurt(target, damage, blocked)
  target.hp = math.max(0, target.hp - damage)
  target.flash = 0.12
  if target.hp <= 0 then
    setState(target, "hurt")
    play("ko")
    endRound(target)
  end
  hitstop = blocked and 0.04 or 0.07
  if blocked then
    play("garde")
    target.stun = 0.12
    return
  end
  play(damage >= 9 and "touche_fort" or "touche")
  setState(target, "hurt")
end

function love.keypressed(key)
  if key == "escape" then love.event.quit() end
  idle = 0
  if screen == "title" and key == "d" then
    startDemo()
    return
  end
  if screen == "result" and key == "return" then
    fighters[1].wins, fighters[2].wins = 0, 0
    game.round, demo = 1, false
    screen = "title"
    playMusic("menu")
    return
  end
  if screen == "title" and key == "return" then
    screen = "select"
    play("choix")
    return
  end
  if screen == "select" then
    if key == "left" or key == "right" then
      cursor.p1 = 1 + (cursor.p1 - 1 + (key == "left" and -1 or 1)) % #FIGHTERS
      play("curseur")
    elseif key == "up" or key == "down" then
      cursor.stage = 1 + cursor.stage % #STAGES
      play("curseur")
    elseif key == "return" then
      play("choix")
      startMatch()
    end
    return
  end
  if key == "j" then strike(fighters[1], false) end
  if key == "k" then strike(fighters[1], true) end
end

function drawBar(fighter, x, flip)
  local w = 108
  love.graphics.setColor(0.1, 0.1, 0.12)
  love.graphics.rectangle("fill", x - 1, 9, w + 2, 9)
  love.graphics.setColor(0.75, 0.2, 0.2)
  local shown = w * fighter.shown / 100
  love.graphics.rectangle("fill", flip and (x + w - shown) or x, 10, shown, 7)
  love.graphics.setColor(0.95, 0.8, 0.25)
  local life = w * fighter.hp / 100
  love.graphics.rectangle("fill", flip and (x + w - life) or x, 10, life, 7)
  love.graphics.setColor(1, 1, 1)
  love.graphics.rectangle("line", x - 1, 9, w + 2, 9)
  love.graphics.printf(fighter.def.name, x, 20, w, flip and "right" or "left")
  for i = 1, WINS_NEEDED do
    local px = flip and (x + w - i * 8) or (x + (i - 1) * 8)
    if fighter.wins >= i then
      love.graphics.circle("fill", px + 3, 33, 3)
    else
      love.graphics.circle("line", px + 3, 33, 3)
    end
  end
end

function drawHud()
  -- a dark band under the bars: white text on a bright forest is unreadable
  love.graphics.setColor(0, 0, 0, 0.45)
  love.graphics.rectangle("fill", 0, 0, W, 40)
  love.graphics.setColor(1, 1, 1)
  drawBar(fighters[1], 8, false)
  drawBar(fighters[2], W - 116, true)
  love.graphics.printf(string.format("%02d", math.ceil(game.clock)), 0, 11, W, "center")
  love.graphics.printf("ROUND " .. game.round, 0, 24, W, "center")
end

function drawFloorShadow(fighter)
  love.graphics.setColor(0, 0, 0, 0.25)
  love.graphics.ellipse("fill", fighter.x, GROUND + 1, 11, 3)
  love.graphics.setColor(1, 1, 1)
end

-- the opponent is not random: it reads the distance, picks a plan, and
-- sticks to it for a fraction of a second, like a hand would
function brainInput(fighter, other, dt)
  local input = { left = false, right = false, up = false, down = false }
  local gap = math.abs(fighter.x - other.x)
  fighter.think = fighter.think - dt

  if fighter.think <= 0 then
    fighter.think = 0.12 + love.math.random() * 0.18
    if other.state == "attack" and gap < fighter.def.reach + 12 then
      fighter.plan = (love.math.random() < 0.3) and "block" or "back"
    elseif fighter.def.arrows and gap > fighter.def.reach + 30 then
      fighter.plan = (love.math.random() < 0.6) and "hit" or "close"
    elseif gap > fighter.def.reach + 10 then
      fighter.plan = "close"
    elseif gap < fighter.def.reach - 8 then
      fighter.plan = (love.math.random() < 0.35) and "back" or "hit"
    else
      fighter.plan = (love.math.random() < 0.85) and "hit" or "wait"
    end
  end

  if fighter.plan == "close" then
    input.left = other.x < fighter.x
    input.right = not input.left
  elseif fighter.plan == "back" or fighter.plan == "block" then
    input.right = other.x < fighter.x
    input.left = not input.right
  elseif fighter.plan == "hit" then
    strike(fighter, love.math.random() < 0.35)
    fighter.plan = "wait"
    fighter.think = 0.3 + love.math.random() * 0.4
  end
  return input
end

function startRound()
  for i, fighter in ipairs(fighters) do
    fighter.hp, fighter.shown, fighter.stun = 100, 100, 0
    fighter.x = (i == 1) and 90 or (W - 90)
    fighter.y, fighter.vy = GROUND, 0
    setState(fighter, "idle")
  end
  game = { round = game and game.round or 1, clock = ROUND_TIME,
           freeze = 2.2, banner = "ROUND " .. (game and game.round or 1),
           over = nil, winner = game and game.winner or nil }
  hitstop = 0
  play("cloche")
  arrows = {}
end

function endRound(loser)
  local winner = (loser == fighters[1]) and fighters[2] or fighters[1]
  winner.wins = winner.wins + 1
  game.over, game.banner = 2.6, "K.O."
  if winner.wins >= WINS_NEEDED then game.winner = winner end
end

function timeUp()
  local a, b = fighters[1], fighters[2]
  game.over, game.banner = 2.6, "TIME UP"
  if a.hp ~= b.hp then
    local winner = (a.hp > b.hp) and a or b
    winner.wins = winner.wins + 1
    if winner.wins >= WINS_NEEDED then game.winner = winner end
  end
end

function drawBanner(text)
  love.graphics.setColor(0, 0, 0, 0.55)
  love.graphics.rectangle("fill", 0, 58, W, 24)
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf(text, 0, 66, W, "center")
end

function playMusic(name)
  for id, source in pairs(music) do
    if id ~= name then source:stop() end
  end
  if music[name] and not music[name]:isPlaying() then music[name]:play() end
end

function drawTitle()
  love.graphics.clear(0.04, 0.05, 0.08)
  love.graphics.setColor(0.42, 0.87, 0.78)
  local title = "F I G H T"
  local police = love.graphics.getFont()
  love.graphics.print(title, (W - police:getWidth(title) * 2) / 2, 40, 0, 2, 2)
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf("written live with love2d", 0, 82, W, "center")
  if math.floor(clock * 2) % 2 == 0 then
    love.graphics.printf("press enter", 0, 108, W, "center")
  end
end

function drawSelect()
  love.graphics.clear(0.04, 0.05, 0.08)
  love.graphics.printf("CHOOSE YOUR FIGHTER", 0, 10, W, "center")
  for i, fighter in ipairs(FIGHTERS) do
    local x, y = 34 + (i - 1) * 68, 84
    love.graphics.setColor(0.12, 0.14, 0.2)
    love.graphics.rectangle("fill", x - 26, y - 44, 52, 56)
    love.graphics.setColor(1, 1, 1)
    local image = art[fighter.id].idle[1]
    love.graphics.draw(image, x, y, 0, 1, 1, image:getWidth() / 2, 29)
    love.graphics.printf(fighter.name, x - 34, y + 16, 68, "center")
    if cursor.p1 == i then
      love.graphics.setColor(0.95, 0.8, 0.25)
      love.graphics.rectangle("line", x - 27, y - 45, 54, 58)
      love.graphics.setColor(1, 1, 1)
    end
  end
  love.graphics.printf("STAGE : " .. STAGES[cursor.stage].name, 0, 120, W, "center")
  love.graphics.printf("arrows : choose      enter : go", 0, 140, W, "center")
end

function startMatch()
  stage = STAGES[cursor.stage]
  repeat cursor.p2 = love.math.random(#FIGHTERS) until cursor.p2 ~= cursor.p1
  fighters = {
    newFighter(FIGHTERS[cursor.p1], 90, 1, { left = "q", right = "d", up = "z", down = "s" }),
    newFighter(FIGHTERS[cursor.p2], W - 90, -1),
  }
  fighters[2].brain = true
  screen = "fight"
  playMusic(cursor.stage == 1 and "fight1" or "fight2")
  startRound()
end

function newArrow(fighter)
  return { x = fighter.x + fighter.facing * 16, y = fighter.y - 34, dir = fighter.facing, life = 3 }
end

function updateArrows(dt)
  for i = #arrows, 1, -1 do
    local arrow = arrows[i]
    arrow.x = arrow.x + arrow.dir * 190 * dt
    arrow.life = arrow.life - dt
    local target = (arrow.dir > 0) and fighters[2] or fighters[1]
    if overlap({ x = arrow.x - 3, y = arrow.y - 2, w = 6, h = 4 }, body(target)) then
      hurt(target, target.blocking and 1 or 6, target.blocking)
      table.remove(arrows, i)
    elseif arrow.life <= 0 or arrow.x < -10 or arrow.x > W + 10 then
      table.remove(arrows, i)
    end
  end
end

function drawArrows()
  love.graphics.setColor(0.95, 0.9, 0.7)
  for _, arrow in ipairs(arrows) do
    love.graphics.rectangle("fill", arrow.x - 5, arrow.y - 1, 10, 2)
  end
  love.graphics.setColor(1, 1, 1)
end

-- an arcade cabinet left alone plays on its own, so both fighters get the
-- same brain, and the machine shows what the game looks like
function startDemo()
  fighters[1].wins, fighters[2].wins = 0, 0
  game.round, idle = 1, 0
  cursor.p1 = love.math.random(#FIGHTERS)
  repeat cursor.p2 = love.math.random(#FIGHTERS) until cursor.p2 ~= cursor.p1
  cursor.stage = love.math.random(#STAGES)
  demo = true
  startMatch()
  fighters[1].brain = true
  fighters[1].controls = nil
end

function drawResult()
  love.graphics.setColor(0, 0, 0, 0.6)
  love.graphics.rectangle("fill", 0, 0, W, H)
  love.graphics.setColor(0.95, 0.8, 0.25)
  love.graphics.printf(game.banner, 0, 60, W, "center")
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf("enter : back to the title", 0, 92, W, "center")
end

-- that's the whole game: four fighters, two stages, three rounds
-- the source goes on github right after the stream
-- thanks for watching

-- the fight ended on a full health bar: the attack was cancelled
-- one frame after it started, by the walking state

-- and now they actually hit each other. that's the game
-- source on github in a minute, thanks for watching

-- still no knockout: they block two hits out of three, so both bars
-- stay full and the round ends on time. let's make them braver

-- there it is: a knockout. that's the whole game, written today
-- source on github in a minute. thanks for watching