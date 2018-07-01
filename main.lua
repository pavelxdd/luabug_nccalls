return function ()
  create_thread(function ()
    coroutine.yield()
  end)
end
