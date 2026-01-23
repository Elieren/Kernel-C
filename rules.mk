# =============================================================================
# Общие правила сборки
# =============================================================================

# Правило для компиляции ассемблерных файлов
$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p  $(dir $@)
	@echo "AS   $<"
	 $(AS) $(ASMFLAGS)  $< -o $@

# Правило для компиляции C файлов
$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p  $(dir $@)
	@echo "CC   $<"
	 $(CC) $(BASE_CFLAGS)  $(EXTRA_CFLAGS) -c $< -o $@

# Автоматическая генерация зависимостей
$(BUILD_DIR)/%.c.d: %.c
	@mkdir -p  $(dir $@)
	 $(CC) $(BASE_CFLAGS) -MM -MT  $(@:.d=.o) $< > $@

# Правило для линковки ядра
 $(BUILD_KERNEL): $(OBJECTS) arch/x86_64/boot/link.ld
	@mkdir -p  $(dir $@)
	@echo "LD   $@"
	 $(LD) $(LDFLAGS) -o  $@ $(OBJECTS)
	@mkdir -p $(BOOT_DIR)
	@cp  $@ $(BOOT_DIR)/
	@echo "Kernel built successfully: $@"