import Testing
@testable import DexLoom

@Suite("DexLoom Core Tests")
struct DexLoomCoreTests {

    @Test("Runtime context creation and destruction")
    func testContextLifecycle() {
        let ctx = dx_context_create()
        #expect(ctx != nil)
        if let ctx = ctx {
            dx_context_destroy(ctx)
        }
    }

    @Test("DEX magic validation rejects invalid data")
    func testDexMagicValidation() {
        // Must be >= header size (112) but with bad magic
        var bad_data = [UInt8](repeating: 0, count: 112)
        var dex: UnsafeMutablePointer<DxDexFile>?
        let result = dx_dex_parse(&bad_data, UInt32(bad_data.count), &dex)
        #expect(result == DX_ERR_INVALID_MAGIC)
    }

    @Test("DEX header parsing with valid minimal header")
    func testDexHeaderParsing() {
        var data = [UInt8](repeating: 0, count: 112)
        let magic: [UInt8] = [0x64, 0x65, 0x78, 0x0A, 0x30, 0x33, 0x35, 0x00]
        for i in 0..<8 { data[i] = magic[i] }
        data[32] = 112; data[33] = 0; data[34] = 0; data[35] = 0
        data[36] = 112; data[37] = 0; data[38] = 0; data[39] = 0
        data[40] = 0x78; data[41] = 0x56; data[42] = 0x34; data[43] = 0x12

        var dex: UnsafeMutablePointer<DxDexFile>?
        let result = dx_dex_parse(&data, UInt32(data.count), &dex)
        #expect(result == DX_OK)
        if let dex = dex {
            #expect(dex.pointee.header.header_size == 112)
            dx_dex_free(dex)
        }
    }

    @Test("Log system does not crash")
    func testLogInit() {
        dx_log_init()
        dx_log_msg(DX_LOG_INFO, "Test", "Hello from test")
    }

    @Test("Result string conversion")
    func testResultStrings() {
        let ok = String(cString: dx_result_string(DX_OK))
        #expect(ok == "OK")
        let notFound = String(cString: dx_result_string(DX_ERR_NOT_FOUND))
        #expect(notFound == "NOT_FOUND")
    }

    @Test("Opcode name lookup")
    func testOpcodeNames() {
        let nop = String(cString: dx_opcode_name(0x00))
        #expect(nop == "nop")
        let invokeVirtual = String(cString: dx_opcode_name(0x6E))
        #expect(invokeVirtual == "invoke-virtual")
    }

    @Test("UI node tree operations")
    func testUINodeTree() {
        let root = dx_ui_node_create(DX_VIEW_LINEAR_LAYOUT, 1)!
        let child1 = dx_ui_node_create(DX_VIEW_TEXT_VIEW, 2)!
        let child2 = dx_ui_node_create(DX_VIEW_BUTTON, 3)!

        dx_ui_node_add_child(root, child1)
        dx_ui_node_add_child(root, child2)
        #expect(root.pointee.child_count == 2)

        dx_ui_node_set_text(child1, "Hello")
        #expect(String(cString: child1.pointee.text) == "Hello")

        let found = dx_ui_node_find_by_id(root, 3)
        #expect(found == child2)
        #expect(dx_ui_node_find_by_id(root, 99) == nil)

        dx_ui_node_destroy(root)
    }

    @Test("VM framework class registration")
    func testVMFrameworkRegistration() {
        let ctx = dx_context_create()!
        let vm = dx_vm_create(ctx)!
        let result = dx_vm_register_framework_classes(vm)
        #expect(result == DX_OK)

        #expect(dx_vm_find_class(vm, "Ljava/lang/Object;") != nil)
        #expect(dx_vm_find_class(vm, "Landroid/app/Activity;") != nil)
        #expect(dx_vm_find_class(vm, "Landroid/widget/TextView;") != nil)
        #expect(dx_vm_find_class(vm, "Landroid/widget/Button;") != nil)

        dx_vm_destroy(vm)
        ctx.pointee.vm = nil
        dx_context_destroy(ctx)
    }

    @Test("VM string creation and retrieval")
    func testVMStrings() {
        let ctx = dx_context_create()!
        let vm = dx_vm_create(ctx)!
        _ = dx_vm_register_framework_classes(vm)

        let strObj = dx_vm_create_string(vm, "Hello DexLoom")
        #expect(strObj != nil)
        if let strObj = strObj {
            let value = dx_vm_get_string_value(strObj)
            #expect(value != nil)
            if let value = value {
                #expect(String(cString: value) == "Hello DexLoom")
            }
        }

        dx_vm_destroy(vm)
        ctx.pointee.vm = nil
        dx_context_destroy(ctx)
    }

    @Test("VM object allocation")
    func testVMObjectAlloc() {
        let ctx = dx_context_create()!
        let vm = dx_vm_create(ctx)!
        _ = dx_vm_register_framework_classes(vm)

        let cls = dx_vm_find_class(vm, "Ljava/lang/Object;")!
        let obj = dx_vm_alloc_object(vm, cls)
        #expect(obj != nil)
        #expect(obj?.pointee.klass == cls)

        dx_vm_destroy(vm)
        ctx.pointee.vm = nil
        dx_context_destroy(ctx)
    }

    @Test("Field set/get on multi-level hierarchy does not crash")
    func testFieldHierarchySafety() {
        let ctx = dx_context_create()!
        let vm = dx_vm_create(ctx)!
        _ = dx_vm_register_framework_classes(vm)

        // Allocate an Activity object (has no field_defs)
        let actCls = dx_vm_find_class(vm, "Landroid/app/Activity;")!
        let obj = dx_vm_alloc_object(vm, actCls)!

        // set_field on a field that doesn't exist should not crash
        var val = DxValue(tag: DX_VAL_INT, DxValue.__Unnamed_union___Anonymous_field1(i: 42))
        let setResult = dx_vm_set_field(obj, "mExtraDataMap", val)
        #expect(setResult == DX_OK) // silently absorbed

        // get_field on a missing field should return null, not crash
        var out = DxValue(tag: DX_VAL_OBJ, DxValue.__Unnamed_union___Anonymous_field1(obj: nil))
        let getResult = dx_vm_get_field(obj, "mExtraDataMap", &out)
        #expect(getResult == DX_OK) // returns null

        dx_vm_destroy(vm)
        ctx.pointee.vm = nil
        dx_context_destroy(ctx)
    }

    @Test("AppCompatActivity is registered")
    func testAppCompatRegistered() {
        let ctx = dx_context_create()!
        let vm = dx_vm_create(ctx)!
        _ = dx_vm_register_framework_classes(vm)

        #expect(dx_vm_find_class(vm, "Landroidx/appcompat/app/AppCompatActivity;") != nil)
        #expect(dx_vm_find_class(vm, "Landroidx/constraintlayout/widget/ConstraintLayout;") != nil)

        dx_vm_destroy(vm)
        ctx.pointee.vm = nil
        dx_context_destroy(ctx)
    }

    @Test("Opcode width lookup")
    func testOpcodeWidths() {
        #expect(dx_opcode_width(0x00) == 1) // nop
        #expect(dx_opcode_width(0x28) == 1) // goto (was broken: 2)
        #expect(dx_opcode_width(0x6E) == 3) // invoke-virtual
        #expect(dx_opcode_width(0x14) == 3) // const (31i)
        #expect(dx_opcode_width(0x18) == 5) // const-wide (51l)
    }

    @Test("Render model creation from UI tree")
    func testRenderModel() {
        let root = dx_ui_node_create(DX_VIEW_LINEAR_LAYOUT, 1)!
        root.pointee.orientation = DX_ORIENTATION_VERTICAL

        let tv = dx_ui_node_create(DX_VIEW_TEXT_VIEW, 2)!
        dx_ui_node_set_text(tv, "Hello")
        dx_ui_node_add_child(root, tv)

        let model = dx_render_model_create(root)
        #expect(model != nil)
        #expect(model!.pointee.root != nil)
        #expect(model!.pointee.root.pointee.type == DX_VIEW_LINEAR_LAYOUT)
        #expect(model!.pointee.root.pointee.child_count == 1)

        dx_render_model_destroy(model)
        dx_ui_node_destroy(root)
    }
}
